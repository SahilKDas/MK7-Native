#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

struct ArmCpuState {
    std::uint32_t R[16]{};
    std::uint32_t cpsr{};
};

struct NdsLinkSlot {
    void* resolved{};
    std::uint32_t epoch{};
    std::uint32_t target{};
    std::uint32_t flags{};
};

inline constexpr unsigned NDS_ARM9 = 0;
inline constexpr unsigned NDS_ARM7 = 1;
inline constexpr std::uint32_t RUNTIME_TRACE_BRANCH = 5;

#define NDS_ARM9_CODE_K(address, sequential) 0u

extern ArmCpuState g_cpu;
extern std::array<std::uint64_t, 2> g_insn_count;
extern unsigned g_nds_active;
extern bool g_insn_hook_armed;

void ctr_runtime_initialize(std::span<std::byte> memory);
void ctr_runtime_reset();
[[nodiscard]] auto ctr_runtime_last_svc() noexcept -> std::uint32_t;
[[nodiscard]] auto ctr_runtime_last_dispatch() noexcept -> std::uint32_t;

[[nodiscard]] auto runtime_should_yield() noexcept -> bool;
[[nodiscard]] auto runtime_unwinding() noexcept -> bool;
void runtime_insn_slow() noexcept;
void runtime_call_push_return(std::uint32_t address) noexcept;
void runtime_call_cancel_return(std::uint32_t address) noexcept;
void runtime_tick(std::uint32_t cycles) noexcept;
void runtime_link_call(NdsLinkSlot* slot) noexcept;
void runtime_swi(std::uint32_t number) noexcept;

[[nodiscard]] auto nds_code_numc(std::uint32_t, std::uint32_t) noexcept -> std::uint32_t;
[[nodiscard]] auto arm9_refill_cycles(std::uint32_t) noexcept -> std::uint32_t;
[[nodiscard]] auto arm7_refill_cycles(std::uint32_t) noexcept -> std::uint32_t;
[[nodiscard]] auto arm9_cycle_combine(
    std::uint32_t code, std::uint32_t data, std::uint32_t internal, std::uint32_t loads) noexcept
    -> std::uint32_t;
[[nodiscard]] auto arm7_cycle_combine(
    std::uint32_t cycles, std::uint32_t data, std::uint32_t loads, bool internal) noexcept
    -> std::uint32_t;

extern "C" void mk7_recomp_block_100000();

