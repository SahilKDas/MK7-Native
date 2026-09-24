#pragma once
#include <array>
#include <cstdint>
#include <span>

struct PicaRasterVertex {
 std::array<float,4> clip{};
 std::array<float,4> color{1,1,1,1};
 std::array<float,2> uv{};
};
struct PicaRasterTexture {
 std::span<const std::uint32_t> rgba{};
 unsigned width{},height{};
 bool linear{},repeat_u{},repeat_v{};
};
struct PicaRasterTarget {
 std::span<std::uint32_t> rgba{};
 std::span<float> depth{};
 std::span<std::uint8_t> stencil{};
 unsigned width{},height{};
};
struct PicaRasterState {
 bool depth_test{},depth_write{},stencil_test{},blend{},texture_enable{};
 std::uint8_t depth_compare{1},stencil_compare{1},stencil_reference{},stencil_mask{255},stencil_write_mask{255};
 std::uint8_t stencil_fail{},stencil_depth_fail{},stencil_pass{2};
 std::uint8_t blend_equation{},blend_source{1},blend_destination{};
 std::uint8_t color_mask{15};
 PicaRasterTexture texture{};
};
bool pica_rasterize_triangle(const std::array<PicaRasterVertex,3>& vertices,PicaRasterTarget target,const PicaRasterState& state) noexcept;
