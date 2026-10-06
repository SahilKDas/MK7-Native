#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <array>
#include <mk7/recomp/pica_raster.hpp>

using PicaVec4 = std::array<float,4>;
struct PicaVertexOutput { std::array<PicaVec4,16> registers{}; };
struct Pica200Snapshot {
 std::uint64_t command_lists{}, register_writes{}, draw_calls{}, draws_rendered{}, draws_unsupported{}, memory_fills{};
 std::uint64_t pixel_producing_draws{}, changed_pixels{}, framebuffer_generation{}, presented_frames{};
 std::uint64_t textured_draws{};
 std::uint64_t rejected_state{}, rejected_bounds{}, rejected_shader{}, rejected_raster{};
 std::uint32_t last_register{}, last_value{};
 std::uint32_t last_texture_config{},last_texture_dimensions{},last_texture_format{},last_texture_address{},last_tev_source{},last_tev_combiner{};
 std::array<std::uint32_t,6> last_tev_sources{},last_tev_operands{},last_tev_combiners{},last_tev_colors{},last_tev_scales{};
 PicaVec4 last_input_position{},last_clip_position{};
 std::array<PicaVec4,7> last_shader_outputs{};
 std::array<std::uint32_t,7> last_output_mappings{};
 std::uint32_t last_shader_entry{},last_output_count{};
};
void pica200_reset(std::span<std::byte> memory) noexcept;
bool pica200_decode_command_list(std::uint32_t address,std::uint32_t size) noexcept;
bool pica200_memory_fill(std::uint32_t start,std::uint32_t end,std::uint32_t value,std::uint16_t control) noexcept;
bool pica200_transfer(std::uint32_t source,std::uint32_t destination,std::uint32_t size) noexcept;
bool pica200_display_transfer(std::uint32_t source,std::uint32_t destination,std::uint32_t input_dimensions,std::uint32_t output_dimensions,std::uint32_t flags) noexcept;
void pica200_set_framebuffer(unsigned screen,std::uint32_t address,std::uint32_t stride,std::uint32_t format) noexcept;
bool pica200_present(unsigned screen,std::span<std::uint32_t> rgba,unsigned width,unsigned height,std::uint64_t* generation=nullptr) noexcept;
void pica200_note_presented(std::uint64_t generation) noexcept;
bool pica200_run_vertex_shader(std::span<const PicaVec4> inputs,PicaVertexOutput& output) noexcept;
Pica200Snapshot pica200_snapshot() noexcept;

bool pica200_shade_and_rasterize_triangle(const std::array<std::span<const PicaVec4>,3>& inputs, PicaRasterTarget target, const PicaRasterState& state, unsigned position_output, unsigned color_output, unsigned uv_output) noexcept;
