#pragma once
#include <cstdint>
void ctr_compact_reset() noexcept;
bool ctr_compact_run_slice(std::uint32_t instruction_budget = 100000u) noexcept;
