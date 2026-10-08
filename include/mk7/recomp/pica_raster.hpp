#pragma once

#include <array>
#include <cstdint>
#include <span>

struct PicaRasterVertex {
  std::array<float, 4> clip{};
  std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
  std::array<float, 2> uv{};
};

struct PicaRasterTexture {
  std::span<const std::uint32_t> rgba{};
  unsigned width{};
  unsigned height{};
  bool linear{};
  bool repeat_u{};
  bool repeat_v{};
};

struct PicaRasterTarget {
  std::span<std::uint32_t> rgba{};
  std::span<float> depth{};
  std::span<std::uint8_t> stencil{};
  unsigned width{};
  unsigned height{};
};

struct PicaTevStage {
  std::uint32_t source{};
  std::uint32_t operand{};
  std::uint32_t combiner{};
  std::uint32_t constant{};
  std::uint32_t scale{};
};

enum class PicaCullMode : std::uint8_t {
  none,
  keep_clockwise,
  keep_counter_clockwise,
};

enum class PicaRasterResult : std::uint8_t {
  rendered,
  invalid_shader_output,
  clipped,
  degenerate,
  culled,
  outside_viewport,
  framebuffer_rejected,
};

struct PicaRasterState {
  bool depth_test{};
  bool depth_write{};
  bool stencil_test{};
  bool blend{};
  bool texture_enable{};
  std::uint8_t depth_compare{1};
  std::uint8_t stencil_compare{1};
  std::uint8_t stencil_reference{};
  std::uint8_t stencil_mask{255};
  std::uint8_t stencil_write_mask{255};
  std::uint8_t stencil_fail{};
  std::uint8_t stencil_depth_fail{};
  std::uint8_t stencil_pass{2};
  std::uint8_t blend_equation{};
  std::uint8_t blend_source{1};
  std::uint8_t blend_destination{};
  std::uint8_t color_mask{15};
  PicaCullMode cull_mode{PicaCullMode::none};
  bool scissor_enable{};
  unsigned scissor_left{};
  unsigned scissor_top{};
  unsigned scissor_right{};
  unsigned scissor_bottom{};
  PicaRasterTexture texture{};
  std::array<PicaTevStage, 6> tev{};
  bool tev_enable{};
};

// Returns the precise pipeline stage that rejected a triangle. Clipping is done
// in homogeneous coordinates, before perspective division, and interpolates
// every varying introduced at a generated clip vertex.
PicaRasterResult pica_rasterize_triangle_detailed(
    const std::array<PicaRasterVertex, 3> &vertices, PicaRasterTarget target,
    const PicaRasterState &state) noexcept;

// Compatibility wrapper for callers that only need accepted/rejected.
bool pica_rasterize_triangle(const std::array<PicaRasterVertex, 3> &vertices,
                             PicaRasterTarget target,
                             const PicaRasterState &state) noexcept;
