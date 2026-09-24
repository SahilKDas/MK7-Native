#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <array>
#include <mk7/recomp/pica_raster.hpp>

using PicaVec4 = std::array<float,4>;
struct PicaVertexOutput { std::array<PicaVec4,16> registers{}; };
struct Pica200Snapshot {
 std::uint64_t command_lists{}, register_writes{}, draw_calls{}, memory_fills{};
 std::uint32_t last_register{}, last_value{};
};
void pica200_reset(std::span<std::byte> memory) noexcept;
bool pica200_decode_command_list(std::uint32_t address,std::uint32_t size) noexcept;
bool pica200_memory_fill(std::uint32_t start,std::uint32_t end,std::uint32_t value,std::uint16_t control) noexcept;
void pica200_set_framebuffer(unsigned screen,std::uint32_t address,std::uint32_t stride,std::uint32_t format) noexcept;
bool pica200_present(unsigned screen,std::span<std::uint32_t> rgba,unsigned width,unsigned height) noexcept;
bool pica200_run_vertex_shader(std::span<const PicaVec4> inputs,PicaVertexOutput& output) noexcept;
Pica200Snapshot pica200_snapshot() noexcept;

bool pica200_shade_and_rasterize_triangle(const std::array<std::span<const PicaVec4>,3>& inputs, PicaRasterTarget target, const PicaRasterState& state, unsigned position_output, unsigned color_output, unsigned uv_output) noexcept;
