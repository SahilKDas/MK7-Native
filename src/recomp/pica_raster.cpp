#include <algorithm>
#include <cmath>
#include <cstddef>
#include <mk7/recomp/pica_raster.hpp>

namespace {
float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
std::array<float, 4> unpack(std::uint32_t color) {
  return {float(color & 255u) / 255.f, float((color >> 8) & 255u) / 255.f,
          float((color >> 16) & 255u) / 255.f,
          float((color >> 24) & 255u) / 255.f};
}
std::uint32_t pack(const std::array<float, 4> &color) {
  std::uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i)
    value |= std::uint32_t(std::lround(clamp01(color[i]) * 255.f)) << (i * 8);
  return value;
}
bool compare(std::uint8_t function, float source, float destination) {
  switch (function & 7u) {
  case 0:
    return false;
  case 1:
    return true;
  case 2:
    return source == destination;
  case 3:
    return source != destination;
  case 4:
    return source < destination;
  case 5:
    return source <= destination;
  case 6:
    return source > destination;
  case 7:
    return source >= destination;
  }
  return false;
}
std::uint8_t stencil_op(std::uint8_t operation, std::uint8_t old,
                        std::uint8_t reference) {
  switch (operation & 7u) {
  case 0:
    return old;
  case 1:
    return 0;
  case 2:
    return reference;
  case 3:
    return old == 255 ? 255 : old + 1;
  case 4:
    return old == 0 ? 0 : old - 1;
  case 5:
    return std::uint8_t(~old);
  case 6:
    return std::uint8_t(old + 1);
  case 7:
    return std::uint8_t(old - 1);
  }
  return old;
}
float blend_factor(std::uint8_t factor, const std::array<float, 4> &source,
                   const std::array<float, 4> &destination,
                   unsigned component) {
  switch (factor) {
  case 0:
    return 0;
  case 1:
    return 1;
  case 2:
    return source[component];
  case 3:
    return 1 - source[component];
  case 4:
    return destination[component];
  case 5:
    return 1 - destination[component];
  case 6:
    return source[3];
  case 7:
    return 1 - source[3];
  case 8:
    return destination[3];
  case 9:
    return 1 - destination[3];
  default:
    return 0;
  }
}
std::array<float, 4> blend(const std::array<float, 4> &source,
                           const std::array<float, 4> &destination,
                           const PicaRasterState &state) {
  if (!state.blend)
    return source;
  std::array<float, 4> result{};
  for (unsigned i = 0; i < 4; ++i) {
    const float a = source[i] *
                    blend_factor(state.blend_source, source, destination, i),
                b = destination[i] * blend_factor(state.blend_destination,
                                                  source, destination, i);
    switch (state.blend_equation) {
    case 0:
      result[i] = a + b;
      break;
    case 1:
      result[i] = a - b;
      break;
    case 2:
      result[i] = b - a;
      break;
    case 3:
      result[i] = std::min(source[i], destination[i]);
      break;
    case 4:
      result[i] = std::max(source[i], destination[i]);
      break;
    default:
      result[i] = source[i];
      break;
    }
  }
  return result;
}
int address_texel(int coordinate, unsigned size, bool repeat) {
  if (!size)
    return 0;
  if (repeat) {
    const int n = int(size);
    return ((coordinate % n) + n) % n;
  }
  return std::clamp(coordinate, 0, int(size) - 1);
}
std::array<float, 4> sample(const PicaRasterTexture &texture, float u,
                            float v) {
  if (!texture.width || !texture.height ||
      texture.rgba.size() < std::size_t(texture.width) * texture.height)
    return {1, 1, 1, 1};
  const float x = u * float(texture.width) - 0.5f,
              y = v * float(texture.height) - 0.5f;
  auto texel = [&](int tx, int ty) {
    const auto ix = address_texel(tx, texture.width, texture.repeat_u),
               iy = address_texel(ty, texture.height, texture.repeat_v);
    return unpack(texture.rgba[std::size_t(iy) * texture.width + ix]);
  };
  if (!texture.linear)
    return texel(int(std::floor(x + 0.5f)), int(std::floor(y + 0.5f)));
  const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
  const float fx = x - x0, fy = y - y0;
  const auto a = texel(x0, y0), b = texel(x0 + 1, y0), c = texel(x0, y0 + 1),
             d = texel(x0 + 1, y0 + 1);
  std::array<float, 4> result{};
  for (unsigned i = 0; i < 4; ++i)
    result[i] = (a[i] * (1 - fx) + b[i] * fx) * (1 - fy) +
                (c[i] * (1 - fx) + d[i] * fx) * fy;
  return result;
}
std::array<float, 4> tev_color(std::uint32_t value) {
  return {float(value & 255u) / 255.f, float((value >> 8) & 255u) / 255.f,
          float((value >> 16) & 255u) / 255.f, float(value >> 24) / 255.f};
}
std::array<float, 4> tev_source(unsigned source,
                                const std::array<float, 4> &primary,
                                const std::array<float, 4> &texture,
                                const std::array<float, 4> &constant,
                                const std::array<float, 4> &previous) {
  switch (source) {
  case 0:
    return primary;
  case 1:
    return {1, 1, 1, 1};
  case 2:
    return {0, 0, 0, 0};
  case 3:
    return texture;
  case 13:
    return {0, 0, 0, 0};
  case 14:
    return constant;
  case 15:
    return previous;
  default:
    return {0, 0, 0, 0};
  }
}
float tev_rgb_operand(const std::array<float, 4> &value, unsigned operand,
                      unsigned component) {
  switch (operand) {
  case 0:
    return value[component];
  case 1:
    return 1 - value[component];
  case 2:
    return value[3];
  case 3:
    return 1 - value[3];
  case 4:
    return value[0];
  case 5:
    return 1 - value[0];
  case 8:
    return value[1];
  case 9:
    return 1 - value[1];
  case 12:
    return value[2];
  case 13:
    return 1 - value[2];
  default:
    return 0;
  }
}
float tev_alpha_operand(const std::array<float, 4> &value, unsigned operand) {
  const unsigned component =
      operand < 2 ? 3 : (operand < 4 ? 0 : (operand < 6 ? 1 : 2));
  const float result = value[component];
  return operand & 1u ? 1 - result : result;
}
float tev_combine(unsigned mode, float a, float b, float c) {
  switch (mode) {
  case 0:
    return a;
  case 1:
    return a * b;
  case 2:
    return a + b;
  case 3:
    return a + b - .5f;
  case 4:
    return a * c + b * (1 - c);
  case 5:
    return a - b;
  case 8:
    return a * b + c;
  case 9:
    return (a + b) * c;
  default:
    return a;
  }
}
std::array<float, 4> run_tev(const PicaRasterState &state,
                             const std::array<float, 4> &primary,
                             const std::array<float, 4> &texture) {
  std::array<float, 4> previous = primary;
  for (const auto &stage : state.tev) {
    const auto constant = tev_color(stage.constant);
    std::array<std::array<float, 4>, 3> rgb_sources{}, alpha_sources{};
    for (unsigned i = 0; i < 3; ++i) {
      rgb_sources[i] = tev_source((stage.source >> (i * 4)) & 15u, primary,
                                  texture, constant, previous);
      alpha_sources[i] = tev_source((stage.source >> (16 + i * 4)) & 15u,
                                    primary, texture, constant, previous);
    }
    std::array<float, 4> next{};
    const auto rgb_mode = stage.combiner & 15u,
               alpha_mode = (stage.combiner >> 16) & 15u;
    if (rgb_mode == 6 || rgb_mode == 7) {
      float dot{};
      for (unsigned component = 0; component < 3; ++component)
        dot +=
            (tev_rgb_operand(rgb_sources[0], stage.operand & 15u, component) *
                 2.f -
             1.f) *
            (tev_rgb_operand(rgb_sources[1], (stage.operand >> 4) & 15u,
                             component) *
                 2.f -
             1.f);
      next[0] = next[1] = next[2] = clamp01(dot);
    } else
      for (unsigned component = 0; component < 3; ++component) {
        const auto a = tev_rgb_operand(rgb_sources[0], stage.operand & 15u,
                                       component),
                   b = tev_rgb_operand(rgb_sources[1],
                                       (stage.operand >> 4) & 15u, component),
                   c = tev_rgb_operand(rgb_sources[2],
                                       (stage.operand >> 8) & 15u, component);
        next[component] = clamp01(tev_combine(rgb_mode, a, b, c));
      }
    const auto a = tev_alpha_operand(alpha_sources[0],
                                     (stage.operand >> 12) & 7u),
               b = tev_alpha_operand(alpha_sources[1],
                                     (stage.operand >> 16) & 7u),
               c = tev_alpha_operand(alpha_sources[2],
                                     (stage.operand >> 20) & 7u);
    next[3] =
        rgb_mode == 7 ? next[0] : clamp01(tev_combine(alpha_mode, a, b, c));
    const float rgb_scale = float(1u << std::min(stage.scale & 3u, 2u)),
                alpha_scale =
                    float(1u << std::min((stage.scale >> 16) & 3u, 2u));
    for (unsigned i = 0; i < 3; ++i)
      next[i] = clamp01(next[i] * rgb_scale);
    next[3] = clamp01(next[3] * alpha_scale);
    previous = next;
  }
  return previous;
}
float edge(float ax, float ay, float bx, float by, float px, float py) {
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

bool finite_vertex(const PicaRasterVertex &vertex) {
  const auto finite = [](float value) { return std::isfinite(value); };
  return std::all_of(vertex.clip.begin(), vertex.clip.end(), finite) &&
         std::all_of(vertex.color.begin(), vertex.color.end(), finite) &&
         std::all_of(vertex.uv.begin(), vertex.uv.end(), finite);
}

PicaRasterVertex interpolate_vertex(const PicaRasterVertex &from,
                                    const PicaRasterVertex &to, float amount) {
  PicaRasterVertex result{};
  for (unsigned component = 0; component < 4; ++component) {
    result.clip[component] =
        std::lerp(from.clip[component], to.clip[component], amount);
    result.color[component] =
        std::lerp(from.color[component], to.color[component], amount);
  }
  for (unsigned component = 0; component < 2; ++component) {
    result.uv[component] =
        std::lerp(from.uv[component], to.uv[component], amount);
  }
  return result;
}

// PICA clip coordinates use -W <= X,Y,Z <= W, with the positive Z half
// removed: -W <= Z <= 0. Each value below is non-negative inside its plane.
float clip_plane_distance(const PicaRasterVertex &vertex, unsigned plane) {
  const auto &position = vertex.clip;
  switch (plane) {
  case 0:
    return position[0] + position[3]; // left
  case 1:
    return position[3] - position[0]; // right
  case 2:
    return position[1] + position[3]; // bottom
  case 3:
    return position[3] - position[1]; // top
  case 4:
    return position[2] + position[3]; // far
  case 5:
    return -position[2]; // near
  default:
    return -1.0f;
  }
}

struct ClippedPolygon {
  // A convex triangle clipped by six planes has at most nine vertices.
  std::array<PicaRasterVertex, 12> vertices{};
  std::size_t size{};
};

ClippedPolygon clip_triangle(const std::array<PicaRasterVertex, 3> &triangle) {
  ClippedPolygon input{};
  std::copy(triangle.begin(), triangle.end(), input.vertices.begin());
  input.size = triangle.size();

  for (unsigned plane = 0; plane < 6 && input.size != 0; ++plane) {
    ClippedPolygon output{};
    for (std::size_t current_index = 0; current_index < input.size;
         ++current_index) {
      const auto &current = input.vertices[current_index];
      const auto &previous =
          input.vertices[(current_index + input.size - 1) % input.size];
      const float current_distance = clip_plane_distance(current, plane);
      const float previous_distance = clip_plane_distance(previous, plane);
      const bool current_inside = current_distance >= 0.0f;
      const bool previous_inside = previous_distance >= 0.0f;

      if (current_inside != previous_inside) {
        const float denominator = previous_distance - current_distance;
        const float amount =
            denominator == 0.0f ? 0.0f : previous_distance / denominator;
        output.vertices[output.size++] =
            interpolate_vertex(previous, current, amount);
      }
      if (current_inside) {
        output.vertices[output.size++] = current;
      }
    }
    input = output;
  }
  return input;
}

bool valid_target(PicaRasterTarget target, const PicaRasterState &state) {
  const auto pixels = std::size_t(target.width) * target.height;
  if (!target.width || !target.height || target.rgba.size() < pixels ||
      target.depth.size() < pixels || target.stencil.size() < pixels) {
    return false;
  }
  if (state.texture_enable &&
      (!state.texture.width || !state.texture.height ||
       state.texture.rgba.size() <
           std::size_t(state.texture.width) * state.texture.height)) {
    return false;
  }
  return !state.blend ||
         (state.blend_equation <= 4 && state.blend_source <= 9 &&
          state.blend_destination <= 9);
}

PicaRasterResult
rasterize_clipped_triangle(const std::array<PicaRasterVertex, 3> &vertices,
                           PicaRasterTarget target,
                           const PicaRasterState &state) {
  constexpr float area_epsilon = 1.0e-6f;
  std::array<float, 3> x{};
  std::array<float, 3> y{};
  std::array<float, 3> inverse_w{};

  for (unsigned vertex = 0; vertex < 3; ++vertex) {
    const auto &clip = vertices[vertex].clip;
    if (!(clip[3] > 0.0f)) {
      return PicaRasterResult::invalid_shader_output;
    }
    inverse_w[vertex] = 1.0f / clip[3];
    const float normalized_x = clip[0] * inverse_w[vertex];
    const float normalized_y = clip[1] * inverse_w[vertex];
    x[vertex] = (normalized_x * 0.5f + 0.5f) * target.width;
    y[vertex] = (1.0f - (normalized_y * 0.5f + 0.5f)) * target.height;
  }

  const float area = edge(x[0], y[0], x[1], y[1], x[2], y[2]);
  if (!std::isfinite(area) || std::abs(area) <= area_epsilon) {
    return PicaRasterResult::degenerate;
  }
  if ((state.cull_mode == PicaCullMode::keep_clockwise && area < 0.0f) ||
      (state.cull_mode == PicaCullMode::keep_counter_clockwise &&
       area > 0.0f)) {
    return PicaRasterResult::culled;
  }

  int minimum_x = std::max(0, int(std::floor(std::min({x[0], x[1], x[2]}))));
  int maximum_x = std::min(int(target.width) - 1,
                           int(std::ceil(std::max({x[0], x[1], x[2]}))));
  int minimum_y = std::max(0, int(std::floor(std::min({y[0], y[1], y[2]}))));
  int maximum_y = std::min(int(target.height) - 1,
                           int(std::ceil(std::max({y[0], y[1], y[2]}))));

  if (state.scissor_enable) {
    minimum_x = std::max(minimum_x, int(state.scissor_left));
    minimum_y = std::max(minimum_y, int(state.scissor_top));
    maximum_x = std::min(maximum_x, int(state.scissor_right));
    maximum_y = std::min(maximum_y, int(state.scissor_bottom));
  }
  if (minimum_x > maximum_x || minimum_y > maximum_y) {
    return PicaRasterResult::outside_viewport;
  }

  for (int pixel_y = minimum_y; pixel_y <= maximum_y; ++pixel_y) {
    for (int pixel_x = minimum_x; pixel_x <= maximum_x; ++pixel_x) {
      const float center_x = pixel_x + 0.5f;
      const float center_y = pixel_y + 0.5f;
      float screen_weights[3]{
          edge(x[1], y[1], x[2], y[2], center_x, center_y) / area,
          edge(x[2], y[2], x[0], y[0], center_x, center_y) / area,
          0.0f,
      };
      screen_weights[2] = 1.0f - screen_weights[0] - screen_weights[1];
      if (screen_weights[0] < 0.0f || screen_weights[1] < 0.0f ||
          screen_weights[2] < 0.0f) {
        continue;
      }

      const float denominator = screen_weights[0] * inverse_w[0] +
                                screen_weights[1] * inverse_w[1] +
                                screen_weights[2] * inverse_w[2];
      if (!(denominator > 0.0f)) {
        continue;
      }
      const float weights[3]{
          screen_weights[0] * inverse_w[0] / denominator,
          screen_weights[1] * inverse_w[1] / denominator,
          screen_weights[2] * inverse_w[2] / denominator,
      };
      const auto offset =
          std::size_t(pixel_y) * target.width + unsigned(pixel_x);
      const float normalized_depth =
          screen_weights[0] * vertices[0].clip[2] * inverse_w[0] +
          screen_weights[1] * vertices[1].clip[2] * inverse_w[1] +
          screen_weights[2] * vertices[2].clip[2] * inverse_w[2];
      const float depth = clamp01(-normalized_depth);

      const auto update_stencil = [&](std::uint8_t operation) {
        const auto old_value = target.stencil[offset];
        const auto new_value =
            stencil_op(operation, old_value, state.stencil_reference);
        target.stencil[offset] =
            std::uint8_t((old_value & ~state.stencil_write_mask) |
                         (new_value & state.stencil_write_mask));
      };
      if (state.stencil_test &&
          !compare(state.stencil_compare,
                   float(state.stencil_reference & state.stencil_mask),
                   float(target.stencil[offset] & state.stencil_mask))) {
        update_stencil(state.stencil_fail);
        continue;
      }
      if (state.depth_test &&
          !compare(state.depth_compare, depth, target.depth[offset])) {
        if (state.stencil_test)
          update_stencil(state.stencil_depth_fail);
        continue;
      }
      if (state.stencil_test)
        update_stencil(state.stencil_pass);

      std::array<float, 4> color{};
      for (unsigned component = 0; component < 4; ++component) {
        for (unsigned vertex = 0; vertex < 3; ++vertex) {
          color[component] +=
              weights[vertex] * vertices[vertex].color[component];
        }
      }
      if (state.texture_enable || state.tev_enable) {
        float u = 0.0f;
        float v = 0.0f;
        for (unsigned vertex = 0; vertex < 3; ++vertex) {
          u += weights[vertex] * vertices[vertex].uv[0];
          v += weights[vertex] * vertices[vertex].uv[1];
        }
        const auto texel = state.texture_enable
                               ? sample(state.texture, u, v)
                               : std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f};
        if (state.tev_enable) {
          color = run_tev(state, color, texel);
        } else {
          for (unsigned component = 0; component < 4; ++component) {
            color[component] *= texel[component];
          }
        }
      }

      const auto destination = unpack(target.rgba[offset]);
      const auto blended = blend(color, destination, state);
      auto result = destination;
      for (unsigned component = 0; component < 4; ++component) {
        if (state.color_mask & (1u << component))
          result[component] = blended[component];
      }
      target.rgba[offset] = pack(result);
      if (state.depth_write)
        target.depth[offset] = depth;
    }
  }
  return PicaRasterResult::rendered;
}
} // namespace

PicaRasterResult pica_rasterize_triangle_detailed(
    const std::array<PicaRasterVertex, 3> &vertices, PicaRasterTarget target,
    const PicaRasterState &state) noexcept {
  if (!valid_target(target, state))
    return PicaRasterResult::framebuffer_rejected;
  if (!std::all_of(vertices.begin(), vertices.end(), finite_vertex)) {
    return PicaRasterResult::invalid_shader_output;
  }

  const auto polygon = clip_triangle(vertices);
  if (polygon.size < 3)
    return PicaRasterResult::clipped;

  PicaRasterResult last_rejection = PicaRasterResult::degenerate;
  bool rendered = false;
  for (std::size_t index = 1; index + 1 < polygon.size; ++index) {
    const std::array<PicaRasterVertex, 3> triangle{polygon.vertices[0],
                                                   polygon.vertices[index],
                                                   polygon.vertices[index + 1]};
    const auto result = rasterize_clipped_triangle(triangle, target, state);
    if (result == PicaRasterResult::rendered) {
      rendered = true;
    } else {
      last_rejection = result;
    }
  }
  return rendered ? PicaRasterResult::rendered : last_rejection;
}

bool pica_rasterize_triangle(const std::array<PicaRasterVertex, 3> &vertices,
                             PicaRasterTarget target,
                             const PicaRasterState &state) noexcept {
  return pica_rasterize_triangle_detailed(vertices, target, state) ==
         PicaRasterResult::rendered;
}
