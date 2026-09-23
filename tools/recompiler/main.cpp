#include "arm_codegen.h"
#include "arm_decode.h"
#include "armv6k_decode.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct Function {
    std::uint32_t address{};
    std::vector<std::uint32_t> instructions;
    std::string name;
    bool thumb{};
};

struct Options {
    std::filesystem::path input;
    std::filesystem::path output;
    std::filesystem::path symbols;
    std::filesystem::path map_output;
    std::uint32_t image_base = 0x00100000;
    std::uint32_t entry = 0x00100000;
    std::uint32_t coverage_permille = 1;
};

[[nodiscard]] std::uint32_t number(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    auto base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        text.remove_prefix(2);
        base = 16;
    } else if (text.find_first_of("abcdefABCDEF") != text.npos) {
        base = 16;
    }
    std::uint32_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::runtime_error("invalid integer: " + std::string(text));
    }
    return value;
}

[[nodiscard]] Options parse_options(int argc, char** argv) {
    Options result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto next = [&]() -> std::string_view {
            if (++index >= argc) throw std::runtime_error("missing value after " + std::string(argument));
            return argv[index];
        };
        if (argument == "--input") result.input = next();
        else if (argument == "--output") result.output = next();
        else if (argument == "--symbols") result.symbols = next();
        else if (argument == "--map-output") result.map_output = next();
        else if (argument == "--image-base") result.image_base = number(next());
        else if (argument == "--entry") result.entry = number(next());
        else if (argument == "--coverage-permille") result.coverage_permille = number(next());
        else throw std::runtime_error("unknown argument: " + std::string(argument));
    }
    if (result.input.empty() || result.output.empty()) {
        throw std::runtime_error("--input and --output are required");
    }
    if (result.coverage_permille == 0 || result.coverage_permille > 1000) {
        throw std::runtime_error("--coverage-permille must be between 1 and 1000");
    }
    if (result.map_output.empty()) {
        result.map_output = result.output;
        result.map_output.replace_extension(".map.csv");
    }
    return result;
}

[[nodiscard]] std::uint32_t read_word(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return std::uint32_t(bytes[offset]) |
           (std::uint32_t(bytes[offset + 1]) << 8) |
           (std::uint32_t(bytes[offset + 2]) << 16) |
           (std::uint32_t(bytes[offset + 3]) << 24);
}

[[nodiscard]] bool in_image(std::span<const std::uint8_t> bytes, const Options& options,
                            std::uint32_t address) {
    return address >= options.image_base &&
           std::uint64_t(address - options.image_base) + 4 <= bytes.size() &&
           (address & 3u) == 0;
}

[[nodiscard]] std::string identifier(std::string name, std::uint32_t address) {
    if (name.empty() || name == "sub") {
        std::ostringstream generated;
        generated << "sub_" << std::hex << address;
        name = generated.str();
    }
    for (auto& character : name) {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_') character = '_';
    }
    if (std::isdigit(static_cast<unsigned char>(name.front()))) name = "_" + name;
    return "mk7_" + name;
}

[[nodiscard]] std::vector<Function> load_symbols(const std::filesystem::path& path) {
    std::vector<Function> functions;
    if (path.empty()) return functions;
    std::ifstream input{path};
    if (!input) throw std::runtime_error("cannot open symbol map");

    std::string line;
    while (std::getline(input, line)) {
        const auto comment = line.find_first_of("#;");
        if (comment != line.npos) line.resize(comment);
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream fields{line};
        std::string address_text;
        std::string second;
        std::string third;
        std::string mode;
        if (!(fields >> address_text >> second)) continue;

        Function function;
        function.address = number(address_text);
        std::uint32_t size{};
        try {
            size = number(second);
            if (!(fields >> third)) third = "sub";
            function.name = third;
        } catch (const std::exception&) {
            function.name = second;
            if (fields >> third) {
                try { size = number(third); } catch (const std::exception&) {}
            }
        }
        if (fields >> mode) function.thumb = mode == "thumb" || mode == "T";
        if (size != 0) {
            const auto stride = function.thumb ? 2u : 4u;
            for (std::uint32_t offset = 0; offset + stride <= size; offset += stride) {
                function.instructions.push_back(function.address + offset);
            }
        }
        functions.push_back(std::move(function));
    }

    std::sort(functions.begin(), functions.end(),
              [](const Function& left, const Function& right) { return left.address < right.address; });
    for (std::size_t index = 0; index < functions.size(); ++index) {
        if (!functions[index].instructions.empty() || index + 1 == functions.size()) continue;
        const auto stride = functions[index].thumb ? 2u : 4u;
        for (auto pc = functions[index].address; pc + stride <= functions[index + 1].address; pc += stride) {
            functions[index].instructions.push_back(pc);
        }
    }
    return functions;
}

struct PendingFunction {
    std::uint32_t address{};
    bool thumb{};
};

[[nodiscard]] std::vector<Function> discover(std::span<const std::uint8_t> bytes,
                                             const Options& options) {
    const auto target_bytes = std::max<std::size_t>(
        64, (bytes.size() * options.coverage_permille + 999) / 1000);
    std::deque<PendingFunction> pending{{options.entry, false}};
    std::set<std::uint64_t> queued{std::uint64_t(options.entry) << 1};
    std::set<std::uint32_t> claimed;
    std::vector<Function> functions;
    std::size_t covered_bytes{};

    while (!pending.empty() && covered_bytes < target_bytes) {
        const auto request = pending.front();
        pending.pop_front();
        if (request.thumb || !in_image(bytes, options, request.address) ||
            claimed.contains(request.address)) {
            continue;
        }

        Function function{.address = request.address, .name = "sub", .thumb = false};
        std::deque<std::uint32_t> blocks{request.address};
        std::set<std::uint32_t> local;
        std::vector<PendingFunction> callees;

        while (!blocks.empty() && local.size() < 4096) {
            auto pc = blocks.front();
            blocks.pop_front();
            while (in_image(bytes, options, pc) && !local.contains(pc) &&
                   !claimed.contains(pc) && local.size() < 4096) {
                local.insert(pc);
                const auto raw = read_word(bytes, pc - options.image_base);
                const auto instruction = armv4t::ArmDecoder::decode(raw, pc);

                // ARM literal loads are how the CTR binary materializes callback and
                // virtual-dispatch targets. Follow only aligned in-image values loaded
                // by an unconditional LDR Rt, [PC, +/-imm12]; the target still has to
                // decode as an instruction before it can become a function seed.
                if ((raw & 0xff7f0000u) == 0xe51f0000u) {
                    const auto offset = raw & 0xfffu;
                    const auto literal = (raw & (1u << 23)) != 0 ? pc + 8u + offset : pc + 8u - offset;
                    if (in_image(bytes, options, literal)) {
                        const auto pointer = read_word(bytes, literal - options.image_base);
                        const auto target = pointer & ~1u;
                        if ((pointer & 1u) == 0 && in_image(bytes, options, target)) {
                            const auto first_raw = read_word(bytes, target - options.image_base);
                            const auto first = armv4t::ArmDecoder::decode(first_raw, target);
                            const bool prologue =
                                (first_raw & 0xffff0000u) == 0xe92d0000u || // stmdb sp!, {...}
                                (first_raw & 0xfffff000u) == 0xe24dd000u || // sub sp, sp, #imm
                                first_raw == 0xe1a0c00du ||                  // mov ip, sp
                                (first_raw & 0xff000000u) == 0xea000000u || // branch veneer
                                (first_raw & 0x0ffffff0u) == 0x012fff10u;   // bx register veneer
                            if (!first.is_undefined && prologue) callees.push_back({target, false});
                        }
                    }
                }

                if (instruction.is_call) {
                    if (!instruction.is_indirect) {
                        const auto target = instruction.branch_target;
                        callees.push_back({target & ~1u, (target & 1u) != 0});
                    }
                    pc += 4;
                    continue;
                }

                if (instruction.is_return || (instruction.is_branch && instruction.is_indirect)) break;
                if (instruction.is_branch) {
                    const auto target = instruction.branch_target & ~1u;
                    if (instruction.cond == armv4t::Cond::AL) {
                        if (target >= request.address && target <= pc) {
                            blocks.push_back(target);
                        } else {
                            callees.push_back({target, (instruction.branch_target & 1u) != 0});
                        }
                        break;
                    }
                    if (in_image(bytes, options, target)) blocks.push_back(target);
                }
                pc += 4;
            }
        }

        if (local.empty()) continue;
        function.instructions.assign(local.begin(), local.end());
        for (const auto pc : function.instructions) claimed.insert(pc);
        covered_bytes += function.instructions.size() * 4;
        functions.push_back(std::move(function));

        for (const auto callee : callees) {
            const auto key = (std::uint64_t(callee.address) << 1) | callee.thumb;
            if (in_image(bytes, options, callee.address) && queued.insert(key).second) {
                pending.push_back(callee);
            }
        }
    }

    std::sort(functions.begin(), functions.end(),
              [](const Function& left, const Function& right) { return left.address < right.address; });
    std::cerr << "mk7-recompile: discovered " << functions.size() << " functions and "
              << covered_bytes << " reachable bytes (" << std::fixed << std::setprecision(4)
              << (100.0 * double(covered_bytes) / double(bytes.size())) << "% of code.bin)\n";
    if (covered_bytes < target_bytes) {
        throw std::runtime_error("reachable function closure did not meet requested coverage");
    }
    return functions;
}

[[nodiscard]] std::string emit_special(const mk7::armv6k::Instruction& instruction) {
    std::ostringstream output;
    using mk7::armv6k::Op;
    if (instruction.op == Op::Rev) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_rev(g_cpu.R[" << unsigned(instruction.rm) << "]);\n";
    else if (instruction.op == Op::Rev16) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_rev16(g_cpu.R[" << unsigned(instruction.rm) << "]);\n";
    else if (instruction.op == Op::Revsh) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_revsh(g_cpu.R[" << unsigned(instruction.rm) << "]);\n";
    else if (instruction.op == Op::Ldrex) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_ldrex(g_cpu.R[" << unsigned(instruction.rn) << "]);\n";
    else if (instruction.op == Op::Strex) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_strex(g_cpu.R[" << unsigned(instruction.rn) << "], g_cpu.R[" << unsigned(instruction.rm) << "]);\n";
    else if (instruction.op == Op::Ldrexh) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_ldrexh(g_cpu.R[" << unsigned(instruction.rn) << "]);\n";
    else if (instruction.op == Op::Strexh) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_strexh(g_cpu.R[" << unsigned(instruction.rn) << "], g_cpu.R[" << unsigned(instruction.rm) << "]);\n";
    else if (instruction.op == Op::Uxtb) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_uxtb(g_cpu.R[" << unsigned(instruction.rm) << "], " << ((instruction.raw >> 10) & 3u) * 8u << ");\n";
    else if (instruction.op == Op::Uxth) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_uxth(g_cpu.R[" << unsigned(instruction.rm) << "], " << ((instruction.raw >> 10) & 3u) * 8u << ");\n";
    else if (instruction.op == Op::Sxtb) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_sxtb(g_cpu.R[" << unsigned(instruction.rm) << "], " << ((instruction.raw >> 10) & 3u) * 8u << ");\n";
    else if (instruction.op == Op::Sxth) output << "g_cpu.R[" << unsigned(instruction.rd) << "] = runtime_sxth(g_cpu.R[" << unsigned(instruction.rm) << "], " << ((instruction.raw >> 10) & 3u) * 8u << ");\n";
    else if (instruction.op == Op::Cps) output << "runtime_cps(" << (instruction.enable ? "true" : "false") << ", 0x" << std::hex << (instruction.raw & 0xe0u) << "u);\n";
    else if (instruction.op == Op::Setend) output << "runtime_setend(" << (instruction.big_endian ? "true" : "false") << ");\n";
    else if (instruction.op == Op::VfpLoadStore) output << "if (arm_cond_passes_i(" << ((instruction.raw >> 28) & 15u) << "u)) runtime_vfp_load_store(0x" << std::hex << instruction.raw << "u);\n";
    return output.str();
}

[[nodiscard]] bool emit(const Options& options) {
    std::ifstream input{options.input, std::ios::binary};
    if (!input) throw std::runtime_error("cannot open code.bin");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};

    auto functions = load_symbols(options.symbols);
    if (functions.empty()) functions = discover(bytes, options);
    if (std::none_of(functions.begin(), functions.end(),
                     [&](const Function& function) { return function.address == options.entry; })) {
        throw std::runtime_error("entry absent from function metadata");
    }

    std::unordered_map<std::uint64_t, std::string> names;
    for (auto& function : functions) {
        function.name = identifier(function.name, function.address);
        names[(std::uint64_t(function.address) << 1) | function.thumb] = function.name;
    }

    std::filesystem::create_directories(options.output.parent_path());
    std::ofstream output{options.output};
    std::ofstream map{options.map_output};
    if (!output || !map) throw std::runtime_error("cannot create generated outputs");

    output << "// Generated locally from the external ROM. Never commit this file.\n"
              "#include <mk7/recomp/runtime.hpp>\n\n";
    map << "function_address,instruction_address,raw,operation,supported\n";

    for (const auto& function : functions) output << "extern \"C\" void " << function.name << "();\n";
    output << "\nextern \"C\" const CtrGeneratedFunction mk7_generated_functions[] = {\n";
    for (const auto& function : functions) {
        output << "  {0x" << std::hex << function.address << "u, "
               << (function.thumb ? "true" : "false") << ", &" << function.name << "},\n";
    }
    output << "};\nextern \"C\" const std::size_t mk7_generated_function_count = "
              "sizeof(mk7_generated_functions)/sizeof(mk7_generated_functions[0]);\n\n";

    bool supported = true;
    std::size_t emitted_bytes{};
    for (const auto& function : functions) {
        if (function.thumb) throw std::runtime_error("Thumb function emission is not implemented yet");
        if (function.instructions.empty()) continue;

        output << "extern \"C\" void " << function.name << "() {\n";
        armv4t::CodegenCtx context;
        context.names_by_key = &names;
        context.current_function_addr = function.address;
        context.current_function_end_addr = function.instructions.back() + 4;
        context.current_function_thumb = false;
        const std::unordered_set<std::uint32_t> local_addresses(function.instructions.begin(), function.instructions.end());
        context.current_instruction_addresses = &local_addresses;

        for (const auto pc : function.instructions) {
            if (!in_image(bytes, options, pc)) throw std::runtime_error("function instruction outside code image");
            const auto raw = read_word(bytes, pc - options.image_base);
            output << "L_" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << pc << ": {\n";
            const auto extension = mk7::armv6k::decode(raw, pc);
            bool not_implemented = false;
            std::string operation;
            if (extension.op != mk7::armv6k::Op::Base) {
                output << emit_special(extension);
                operation = "armv6k";
            } else {
                const auto instruction = armv4t::ArmDecoder::decode(raw, pc);
                operation = armv4t::ir_op_name(instruction.op);
                output << armv4t::ArmCodegen::emit_instr(instruction, context, &not_implemented);
                not_implemented = not_implemented || instruction.is_undefined;
            }
            output << "}\n";
            supported = supported && !not_implemented;
            emitted_bytes += 4;
            map << "0x" << std::hex << function.address << ",0x" << pc << ",0x"
                << std::setw(8) << std::setfill('0') << raw << ',' << operation << ','
                << (not_implemented ? "false" : "true") << '\n';
        }
        output << "}\n\n";
    }

    output << "extern \"C\" void mk7_recomp_block_100000(){ "
           << names[(std::uint64_t(options.entry) << 1)] << "(); }\n";
    std::cerr << "mk7-recompile: emitted " << std::dec << emitted_bytes << " mapped bytes across "
              << functions.size() << " functions\n";
    return supported;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return emit(parse_options(argc, argv)) ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "mk7-recompile: " << error.what() << '\n';
        return 2;
    }
}
