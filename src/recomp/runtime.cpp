#include <mk7/recomp/runtime.hpp>

#include <iostream>
#include <vector>

ArmCpuState g_cpu{};
std::array<std::uint64_t, 2> g_insn_count{};
unsigned g_nds_active = NDS_ARM9;
bool g_insn_hook_armed = false;

namespace {

std::span<std::byte> g_memory;
std::vector<std::uint32_t> g_return_stack;
bool g_unwinding = false;
std::uint32_t g_last_svc = 0xffffffffu;
std::uint32_t g_last_dispatch = 0xffffffffu;

} // namespace

void ctr_runtime_initialize(std::span<std::byte> memory) {
    g_memory = memory;
    ctr_runtime_reset();
}

void ctr_runtime_reset() {
    g_cpu = {};
    g_cpu.cpsr = 0x10u;
    g_insn_count = {};
    g_return_stack.clear();
    g_unwinding = false;
    g_last_svc = 0xffffffffu;
    g_last_dispatch = 0xffffffffu;
}

auto ctr_runtime_last_svc() noexcept -> std::uint32_t { return g_last_svc; }
auto ctr_runtime_last_dispatch() noexcept -> std::uint32_t { return g_last_dispatch; }
auto runtime_should_yield() noexcept -> bool { return false; }
auto runtime_unwinding() noexcept -> bool { return g_unwinding; }
void runtime_insn_slow() noexcept {}

void runtime_call_push_return(std::uint32_t address) noexcept {
    g_return_stack.push_back(address);
}

void runtime_call_cancel_return(std::uint32_t address) noexcept {
    if (!g_return_stack.empty() && g_return_stack.back() == address) {
        g_return_stack.pop_back();
    }
}

void runtime_tick(std::uint32_t) noexcept {}

void runtime_link_call(NdsLinkSlot* slot) noexcept {
    g_last_dispatch = slot->target;
    g_unwinding = true;
    std::cout << "[ctr] deferred dispatch to unregistered guest block 0x"
              << std::hex << slot->target << std::dec << '\n';
}

void runtime_swi(std::uint32_t number) noexcept {
    g_last_svc = number;
    // Unknown services return a normal CTR failure Result in R0. The dispatcher
    // never throws or dereferences guest pointers until a service is modeled.
    g_cpu.R[0] = 0xd8e007f7u;
    std::cout << "[ctr:srv] SVC 0x" << std::hex << number
              << " handled with Result 0x" << g_cpu.R[0] << std::dec << '\n';
}

auto nds_code_numc(std::uint32_t, std::uint32_t) noexcept -> std::uint32_t { return 1; }
auto arm9_refill_cycles(std::uint32_t) noexcept -> std::uint32_t { return 1; }
auto arm7_refill_cycles(std::uint32_t) noexcept -> std::uint32_t { return 1; }

auto arm9_cycle_combine(
    std::uint32_t code, std::uint32_t data, std::uint32_t internal, std::uint32_t loads) noexcept
    -> std::uint32_t {
    return code + data + internal + loads;
}

auto arm7_cycle_combine(
    std::uint32_t cycles, std::uint32_t data, std::uint32_t loads, bool internal) noexcept
    -> std::uint32_t {
    return cycles + data + loads + static_cast<std::uint32_t>(internal);
}

