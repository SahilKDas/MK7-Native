#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

struct Pica200Snapshot {
 std::uint64_t command_lists{}, register_writes{}, draw_calls{}, memory_fills{};
 std::uint32_t last_register{}, last_value{};
};
void pica200_reset(std::span<std::byte> memory) noexcept;
bool pica200_decode_command_list(std::uint32_t address,std::uint32_t size) noexcept;
bool pica200_memory_fill(std::uint32_t start,std::uint32_t end,std::uint32_t value,std::uint16_t control) noexcept;
void pica200_set_framebuffer(unsigned screen,std::uint32_t address,std::uint32_t stride,std::uint32_t format) noexcept;
bool pica200_present(unsigned screen,std::span<std::uint32_t> rgba,unsigned width,unsigned height) noexcept;
Pica200Snapshot pica200_snapshot() noexcept;
