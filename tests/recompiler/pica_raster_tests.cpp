#include <array>
#include <cassert>
#include <limits>
#include <mk7/recomp/pica_raster.hpp>
#include <vector>

int main() {
  std::vector<std::uint32_t> rgba(64, 0xff000000u);
  std::vector<float> depth(64, 1.f);
  std::vector<std::uint8_t> stencil(64, 0);
  PicaRasterTarget target{rgba, depth, stencil, 8, 8};
  std::array<PicaRasterVertex, 3> triangle{};
  triangle[0].clip = {-0.8f, -0.8f, -0.25f, 1.f};
  triangle[1].clip = {0.8f, -0.8f, -0.25f, 1.f};
  triangle[2].clip = {0.f, 0.8f, -0.25f, 1.f};
  for (auto &vertex : triangle)
    vertex.color = {1, 0, 0, 1};
  PicaRasterState state{};
  state.depth_test = true;
  state.depth_write = true;
  state.depth_compare = 4;
  assert(pica_rasterize_triangle(triangle, target, state));
  assert(rgba[4 * 8 + 4] == 0xff0000ffu && rgba[0] == 0xff000000u);
  assert(depth[4 * 8 + 4] == 0.25f);
  for (auto &vertex : triangle)
    vertex.clip[2] = -0.75f;
  for (auto &vertex : triangle)
    vertex.color = {0, 1, 0, 1};
  assert(pica_rasterize_triangle(triangle, target, state));
  assert(rgba[4 * 8 + 4] == 0xff0000ffu);
  state.stencil_test = true;
  state.stencil_compare = 2;
  state.stencil_reference = 1;
  state.depth_test = false;
  assert(pica_rasterize_triangle(triangle, target, state));
  assert(rgba[4 * 8 + 4] == 0xff0000ffu);
  state.stencil_compare = 1;
  state.stencil_pass = 2;
  state.stencil_reference = 7;
  state.depth_write = false;
  state.blend = true;
  state.blend_source = 6;
  state.blend_destination = 7;
  for (auto &vertex : triangle)
    vertex.color = {0, 1, 0, .5f};
  assert(pica_rasterize_triangle(triangle, target, state));
  assert(stencil[4 * 8 + 4] == 7);
  const auto blended = rgba[4 * 8 + 4];
  assert((blended & 255u) >= 126u && (blended & 255u) <= 129u);
  assert(((blended >> 8) & 255u) >= 126u && ((blended >> 8) & 255u) <= 129u);
  std::array<std::uint32_t, 4> texture{0xff0000ffu, 0xff00ff00u, 0xffff0000u,
                                       0xffffffffu};
  state.blend = false;
  state.stencil_test = false;
  state.texture_enable = true;
  state.texture = {texture, 2, 2, false, false, false};
  for (auto &vertex : triangle) {
    vertex.color = {1, 1, 1, 1};
    vertex.uv = {.25f, .25f};
  }
  assert(pica_rasterize_triangle(triangle, target, state));
  assert(rgba[4 * 8 + 4] == 0xff0000ffu);
  state.tev_enable = true;
  state.tev[0].source = 0x00300030u;
  state.tev[0].combiner = 0x00010001u;
  for (unsigned stage = 1; stage < 6; ++stage)
    state.tev[stage].source = 0x0fff0fffu;
  for (auto &vertex : triangle)
    vertex.color = {.5f, 1.f, 1.f, .5f};
  assert(pica_rasterize_triangle(triangle, target, state));
  const auto combined = rgba[4 * 8 + 4];
  assert((combined & 255u) >= 126u && (combined & 255u) <= 129u);
  assert(((combined >> 24) & 255u) >= 126u &&
         ((combined >> 24) & 255u) <= 129u);
  state.texture.rgba = {};
  assert(!pica_rasterize_triangle(triangle, target, state));

  PicaRasterState classification_state{};
  for (auto &vertex : triangle) {
    vertex.color = {1, 0, 0, 1};
    vertex.uv = {};
    vertex.clip[2] = -.25f;
    vertex.clip[3] = 1.f;
  }
  triangle[0].clip[0] = -0.8f;
  triangle[0].clip[1] = -0.8f;
  triangle[1].clip[0] = 2.0f;
  triangle[1].clip[1] = -0.8f;
  triangle[2].clip[0] = 0.0f;
  triangle[2].clip[1] = 0.8f;
  assert(pica_rasterize_triangle_detailed(triangle, target,
                                          classification_state) ==
         PicaRasterResult::rendered);

  for (auto &vertex : triangle)
    vertex.clip[0] = 2.f;
  assert(pica_rasterize_triangle_detailed(triangle, target,
                                          classification_state) ==
         PicaRasterResult::clipped);

  triangle[0].clip = {-.8f, -.8f, -.25f, 1.f};
  triangle[1].clip = {.8f, -.8f, -.25f, 1.f};
  triangle[2].clip = {0.f, .8f, -.25f, 1.f};
  triangle[0].clip[0] = std::numeric_limits<float>::quiet_NaN();
  assert(pica_rasterize_triangle_detailed(triangle, target,
                                          classification_state) ==
         PicaRasterResult::invalid_shader_output);

  triangle[0].clip = triangle[1].clip =
      triangle[2].clip = {0.f, 0.f, -.25f, 1.f};
  assert(pica_rasterize_triangle_detailed(triangle, target,
                                          classification_state) ==
         PicaRasterResult::degenerate);

  triangle[0].clip = {-.8f, -.8f, -.25f, 1.f};
  triangle[1].clip = {.8f, -.8f, -.25f, 1.f};
  triangle[2].clip = {0.f, .8f, -.25f, 1.f};
  classification_state.cull_mode = PicaCullMode::keep_counter_clockwise;
  assert(pica_rasterize_triangle_detailed(triangle, target,
                                          classification_state) ==
         PicaRasterResult::culled);

  classification_state.cull_mode = PicaCullMode::none;
  classification_state.scissor_enable = true;
  classification_state.scissor_left = classification_state.scissor_top = 100;
  classification_state.scissor_right = classification_state.scissor_bottom =
      101;
  assert(pica_rasterize_triangle_detailed(triangle, target,
                                          classification_state) ==
         PicaRasterResult::outside_viewport);

  PicaRasterTarget invalid_target{};
  assert(pica_rasterize_triangle_detailed(triangle, invalid_target, {}) ==
         PicaRasterResult::framebuffer_rejected);
}
