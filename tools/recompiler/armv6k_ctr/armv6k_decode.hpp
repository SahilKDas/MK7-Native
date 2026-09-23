#pragma once
#include <cstdint>
namespace mk7::armv6k {
enum class Op { Base, Rev, Rev16, Revsh, Ldrex, Strex, Uxtb, Uxth, Sxtb, Sxth, Cps, Setend };
struct Instruction { Op op{Op::Base}; std::uint32_t raw{}; std::uint32_t pc{}; std::uint8_t rd{}, rn{}, rm{}; bool enable{}, big_endian{}; };
[[nodiscard]] Instruction decode(std::uint32_t word, std::uint32_t pc) noexcept;
}
