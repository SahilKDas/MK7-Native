#include <mk7/recomp/runtime.hpp>

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#include <bcrypt.h>
#endif

namespace {

constexpr std::string_view expected_sha512 =
    "8b10af4bf0347d4ed6076d4b29f36e6d2ab76f6ef60459958ac72842b7345877"
    "fb4721992c04664149edce7c294715803d4261b93a9553ef6e85616b71ce0225";
constexpr std::size_t ctr_application_memory_size = 64u * 1024u * 1024u;
constexpr std::uint32_t text_address = 0x00100000u;
constexpr std::size_t expected_code_size = 0x00577000u;

struct Options {
    std::filesystem::path cia;
    std::filesystem::path ctrtool = "ctrtool";
};

class TempDirectory {
public:
    TempDirectory()
        : path_{std::filesystem::temp_directory_path() /
                ("mk7-native-exefs-" + std::to_string(GetCurrentProcessId()))} {
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() { std::filesystem::remove_all(path_); }

    TempDirectory(const TempDirectory&) = delete;
    auto operator=(const TempDirectory&) -> TempDirectory& = delete;

    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path& { return path_; }

private:
    std::filesystem::path path_;
};

[[nodiscard]] auto options_from(int argc, char** argv) -> Options {
    if (argc < 2) throw std::runtime_error{"missing CIA path"};
    Options options{.cia = argv[1]};
    for (auto index = 2; index < argc; ++index) {
        if (std::string_view{argv[index]} == "--ctrtool" && index + 1 < argc) {
            options.ctrtool = argv[++index];
        } else {
            throw std::runtime_error{"unknown argument: " + std::string{argv[index]}};
        }
    }
    return options;
}

#ifdef _WIN32
[[nodiscard]] auto sha512(const std::filesystem::path& path) -> std::string {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD object_size{};
    DWORD result_size{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA512_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &result_size, 0) < 0) {
        throw std::runtime_error{"unable to initialize SHA-512"};
    }
    std::vector<UCHAR> object(object_size);
    std::array<UCHAR, 64> digest{};
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error{"unable to create SHA-512 context"};
    }

    std::ifstream input{path, std::ios::binary};
    if (!input) throw std::runtime_error{"unable to open CIA"};
    std::array<char, 1024 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                           static_cast<ULONG>(count), 0) < 0) {
            throw std::runtime_error{"SHA-512 update failed"};
        }
    }
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
        throw std::runtime_error{"SHA-512 finalization failed"};
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto byte : digest) result << std::setw(2) << static_cast<unsigned>(byte);
    return result.str();
}

auto run_ctrtool(const Options& options, const std::filesystem::path& output) -> int {
    const auto executable = options.ctrtool.wstring();
    const auto quote = [](const std::wstring& value) {
        if (value.contains(L'"')) throw std::runtime_error{"quotes are not supported in paths"};
        return L"\"" + value + L"\"";
    };
    auto command = quote(executable) + L" --quiet --decompresscode --exefsdir=" +
                   quote(output.wstring()) + L" " + quote(options.cia.wstring());
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command_buffer.data(), nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &startup, &process)) {
        throw std::runtime_error{"unable to start ctrtool"};
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code{};
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exit_code);
}
#else
#error mk7-run currently requires Windows BCrypt and process APIs
#endif

[[nodiscard]] auto read_file(const std::filesystem::path& path) -> std::vector<std::byte> {
    std::ifstream input{path, std::ios::binary};
    if (!input) throw std::runtime_error{"unable to open extracted code"};
    const std::vector<char> chars{
        std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    return std::vector<std::byte>{std::as_bytes(std::span{chars}).begin(),
                                  std::as_bytes(std::span{chars}).end()};
}

} // namespace

auto main(int argc, char** argv) -> int {
    try {
        const auto options = options_from(argc, argv);
        std::cout << "[verify] SHA-512...\n";
        const auto digest = sha512(options.cia);
        if (digest != expected_sha512) {
            throw std::runtime_error{"unsupported CIA SHA-512: " + digest};
        }
        std::cout << "[verify] supported USA Rev2 image\n";

        const TempDirectory extraction;
        if (run_ctrtool(options, extraction.path()) != 0) {
            throw std::runtime_error{"ctrtool extraction failed"};
        }

        const auto code = read_file(extraction.path() / "code.bin");
        if (code.size() != expected_code_size) {
            throw std::runtime_error{"unexpected decompressed code size"};
        }

        std::vector<std::byte> memory(ctr_application_memory_size);
        std::copy(code.begin(), code.end(), memory.begin() + text_address);
        ctr_runtime_initialize(memory);
        std::cout << "[memory] initialized 64 MiB CTR application map\n";

        mk7_recomp_block_100000();
        std::cout << "[recomp] entry block executed; next PC=0x" << std::hex
                  << ctr_runtime_last_dispatch() << std::dec << '\n';

        // Exercise the same fail-closed dispatcher used by generated SVC
        // instructions until the entry closure reaches its first real SVC.
        runtime_swi(0);
        std::cout << "[boot] first service boundary completed without a crash\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "mk7-run: " << error.what() << '\n'
                  << "usage: mk7-run <game.cia> [--ctrtool path/to/ctrtool]\n";
        return 1;
    }
}

