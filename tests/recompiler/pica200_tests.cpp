#include <array>
#include <cassert>
#include <cstring>
#include <mk7/recomp/pica200.hpp>
#include <vector>

int main() {
  std::vector<std::byte> memory(0x8000);
  pica200_reset(memory);
  const std::uint32_t parameter0 = 0x11223344, header0 = 0x000f0041;
  const std::uint32_t parameter1 = 1, header1 = 0x000f022e;
  std::memcpy(memory.data() + 0x100, &parameter0, 4);
  std::memcpy(memory.data() + 0x104, &header0, 4);
  std::memcpy(memory.data() + 0x108, &parameter1, 4);
  std::memcpy(memory.data() + 0x10c, &header1, 4);
  assert(pica200_decode_command_list(0x100, 16));
  auto state = pica200_snapshot();
  assert(state.command_lists == 1 && state.register_writes == 2 &&
         state.draw_calls == 1 && state.last_register == 0x22e);
  assert(pica200_memory_fill(0x1000, 0x1010, 0xff336699, 2));
  for (unsigned i = 0; i < 16; i += 4) {
    std::uint32_t value{};
    std::memcpy(&value, memory.data() + 0x1000 + i, 4);
    assert(value == 0xff336699);
  }
  assert(pica200_memory_fill(0x1f000000, 0x1f000010, 0xff224466, 2));
  assert(pica200_transfer(0x1f000000, 0x2100, 16));
  for (unsigned i = 0; i < 16; i += 4) {
    std::uint32_t value{};
    std::memcpy(&value, memory.data() + 0x2100 + i, 4);
    assert(value == 0xff224466);
  }
  assert(pica200_display_transfer(0x2100, 0x1f000100, 2u << 16 | 2u,
                                  2u << 16 | 2u, 0));
  pica200_set_framebuffer(1, 0x1f000100, 8, 0);
  pica200_complete_framebuffer();
  std::array<std::uint32_t, 4> vram_output{};
  assert(pica200_present(1, vram_output, 2, 2));
  assert(vram_output[0] == 0xff224466);
  const std::uint32_t nested_parameter = 0xa5a55a5au,
                      nested_header = 0x000f0041u;
  std::memcpy(memory.data() + 0x2200, &nested_parameter, 4);
  std::memcpy(memory.data() + 0x2204, &nested_header, 4);
  assert(pica200_transfer(0x2200, 0x1f000200, 8));
  const std::uint32_t indirect_words[6]{1u,          0x000f0238u, 0x03000040u,
                                        0x000f023au, 1u,          0x000f023cu};
  std::memcpy(memory.data() + 0x2300, indirect_words, sizeof(indirect_words));
  const auto before_indirect = pica200_snapshot();
  assert(pica200_decode_command_list(0x2300, sizeof(indirect_words)));
  const auto after_indirect = pica200_snapshot();
  assert(after_indirect.command_lists == before_indirect.command_lists + 2 &&
         after_indirect.last_register == 0x41u &&
         after_indirect.last_value == nested_parameter);
  const std::uint32_t pixels[4]{0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff};
  std::memcpy(memory.data() + 0x2000, pixels, sizeof(pixels));
  pica200_set_framebuffer(0, 0x2000, 8, 0);
  pica200_complete_framebuffer();
  std::array<std::uint32_t, 1> output{};
  assert(pica200_present(0, output, 1, 1));
  assert(output[0] == pixels[0]);
  auto gpu_write = [&](unsigned &cursor, std::uint32_t reg,
                       std::uint32_t value) {
    const std::uint32_t header = reg | 0x000f0000u;
    std::memcpy(memory.data() + cursor, &value, 4);
    std::memcpy(memory.data() + cursor + 4, &header, 4);
    cursor += 8;
  };
  unsigned cursor = 0x300;
  gpu_write(cursor, 0x2cb, 0);
  gpu_write(cursor, 0x2cc, (0x13u << 26) | 0u);
  gpu_write(cursor, 0x2cc, 0x22u << 26);
  gpu_write(cursor, 0x2d5, 0);
  gpu_write(cursor, 0x2d6, 0x0000036fu);
  gpu_write(cursor, 0x2ba, 0);
  assert(pica200_decode_command_list(0x300, cursor - 0x300));
  const PicaVec4 vertex{1.25f, 2.5f, -3.f, 1.f};
  PicaVertexOutput shaded{};
  assert(
      pica200_run_vertex_shader(std::span<const PicaVec4>(&vertex, 1), shaded));
  assert(shaded.registers[0] == vertex);
  std::array<PicaVec4, 3> triangle_inputs{{{-0.8f, -0.8f, -0.25f, 1.f},
                                           {0.8f, -0.8f, -0.25f, 1.f},
                                           {0.f, 0.8f, -0.25f, 1.f}}};
  std::array<std::span<const PicaVec4>, 3> input_spans{};
  for (unsigned i = 0; i < 3; ++i)
    input_spans[i] = std::span<const PicaVec4>(&triangle_inputs[i], 1);
  std::array<std::uint32_t, 64> triangle_pixels{};
  std::array<float, 64> triangle_depth{};
  triangle_depth.fill(1.f);
  std::array<std::uint8_t, 64> triangle_stencil{};
  const PicaRasterTarget triangle_target{triangle_pixels, triangle_depth,
                                         triangle_stencil, 8, 8};
  assert(pica200_shade_and_rasterize_triangle(input_spans, triangle_target, {},
                                              0, 0, 0));
  assert(triangle_pixels[4 * 8 + 4] != 0);
  assert(!pica200_shade_and_rasterize_triangle(input_spans, triangle_target, {},
                                               16, 0, 0));
  gpu_write(cursor, 0x2cb, 0);
  gpu_write(cursor, 0x2cc, (0x13u << 26) | 1u);
  gpu_write(cursor, 0x2cc, 0x22u << 26);
  gpu_write(cursor, 0x2d5, 1);
  gpu_write(cursor, 0x2d6, 0x00000aa8u);
  assert(pica200_decode_command_list(cursor - 40, 40));
  assert(
      pica200_run_vertex_shader(std::span<const PicaVec4>(&vertex, 1), shaded));
  assert(shaded.registers[0][0] == vertex[1] && shaded.registers[0][1] == 0.f);
  const auto uniform_cursor = cursor;
  const std::uint32_t packed_f24[3]{0x00410000u, 0x80004000u, 0x3f000040u};
  gpu_write(cursor, 0x2c0, 0);
  for (const auto word : packed_f24)
    gpu_write(cursor, 0x2c1, word);
  gpu_write(cursor, 0x2cb, 0);
  gpu_write(cursor, 0x2cc, (0x13u << 26) | (32u << 12));
  gpu_write(cursor, 0x2cc, 0x22u << 26);
  gpu_write(cursor, 0x2d5, 0);
  gpu_write(cursor, 0x2d6, 0x0000036fu);
  assert(pica200_decode_command_list(uniform_cursor, cursor - uniform_cursor));
  assert(pica200_run_vertex_shader({}, shaded));
  assert(shaded.registers[0][0] == 1.f && shaded.registers[0][1] == 2.f &&
         shaded.registers[0][2] == 3.f && shaded.registers[0][3] == 4.f);

  // Relative constant addressing wraps within c0-c127. The reserved c96-c127
  // range reads as one and must never alias input or temporary registers.
  const auto relative_cursor = cursor;
  gpu_write(cursor, 0x2b1, 0x00000100u); // i0.y = 1
  gpu_write(cursor, 0x2cb, 0);
  gpu_write(cursor, 0x2cc, 0x29u << 26); // LOOP, one iteration, aL = 1
  gpu_write(cursor, 0x2cc, (0x13u << 26) | (3u << 19) | (127u << 12));
  gpu_write(cursor, 0x2cc, 0x22u << 26);
  gpu_write(cursor, 0x2d5, 0);
  gpu_write(cursor, 0x2d6, 0x0000036fu);
  gpu_write(cursor, 0x2ba, 0);
  assert(
      pica200_decode_command_list(relative_cursor, cursor - relative_cursor));
  assert(pica200_run_vertex_shader({}, shaded));
  assert(shaded.registers[0] == PicaVec4({1.f, 1.f, 1.f, 1.f}));
  assert(!pica200_decode_command_list(0x7ffc, 16));
  gpu_write(cursor, 0x2cb, 0);
  gpu_write(cursor, 0x2cc, 0x10u << 26);
  assert(pica200_decode_command_list(cursor - 16, 16));
  assert(!pica200_run_vertex_shader(std::span<const PicaVec4>(&vertex, 1),
                                    shaded));
  pica200_reset(memory);
  const PicaVec4 guest_vertices[3]{{-.8f, -.8f, -.25f, 1.f},
                                   {.8f, -.8f, -.25f, 1.f},
                                   {0.f, .8f, -.25f, 1.f}};
  std::memcpy(memory.data() + 0x5000, guest_vertices, sizeof(guest_vertices));
  unsigned draw_cursor = 0x600;
  gpu_write(draw_cursor, 0x200, 0x5000u >> 3);
  gpu_write(draw_cursor, 0x201, 0x0f);
  gpu_write(draw_cursor, 0x202, 0);
  gpu_write(draw_cursor, 0x203, 0);
  gpu_write(draw_cursor, 0x204, 0);
  gpu_write(draw_cursor, 0x205, (1u << 28) | (16u << 16));
  gpu_write(draw_cursor, 0x228, 3);
  gpu_write(draw_cursor, 0x22a, 0);
  gpu_write(draw_cursor, 0x11d, 0x4000u >> 3);
  gpu_write(draw_cursor, 0x11e, 8u | (7u << 12));
  gpu_write(draw_cursor, 0x117, 2);
  gpu_write(draw_cursor, 0x107, 0xf00);
  gpu_write(draw_cursor, 0x2cb, 0);
  gpu_write(draw_cursor, 0x2cc, 0x13u << 26);
  gpu_write(draw_cursor, 0x2cc, 0x22u << 26);
  gpu_write(draw_cursor, 0x2d5, 0);
  gpu_write(draw_cursor, 0x2d6, 0x0000036fu);
  gpu_write(draw_cursor, 0x2ba, 0);
  gpu_write(draw_cursor, 0x22e, 1);
  assert(pica200_decode_command_list(0x600, draw_cursor - 0x600));
  auto drawn = pica200_snapshot();
  assert(drawn.draw_calls == 1 && drawn.draws_rendered == 1 &&
         drawn.draws_unsupported == 0);
  assert(drawn.pixel_producing_draws == 1 && drawn.changed_pixels > 0 &&
         drawn.framebuffer_generation == 1);
  pica200_complete_framebuffer();
  std::uint64_t generation{};
  std::array<std::uint32_t, 64> presented{};
  assert(pica200_present(0, presented, 8, 8, &generation));
  assert(generation == 1);
  pica200_note_presented(generation);
  assert(pica200_snapshot().presented_frames == 1);
  std::uint32_t guest_pixel{};
  std::memcpy(&guest_pixel, memory.data() + 0x4000 + (4 * 8 + 4) * 4, 4);
  assert(guest_pixel == 0xffffffffu);
  gpu_write(draw_cursor, 0x201, 0);
  gpu_write(draw_cursor, 0x22e, 1);
  assert(pica200_decode_command_list(draw_cursor - 16, 16));
  drawn = pica200_snapshot();
  assert(drawn.draw_calls == 2 && drawn.draws_rendered == 1 &&
         drawn.draws_unsupported == 1);
  const std::uint8_t indices[3]{0, 1, 2};
  std::memcpy(memory.data() + 0x5030, indices, sizeof(indices));
  gpu_write(draw_cursor, 0x201, 0x0f);
  gpu_write(draw_cursor, 0x227, 0x30);
  gpu_write(draw_cursor, 0x22f, 1);
  assert(pica200_decode_command_list(draw_cursor - 24, 24));
  drawn = pica200_snapshot();
  assert(drawn.draw_calls == 3 && drawn.draws_rendered == 2);

  // A real draw can source attributes from independent loaders, using each
  // loader's own offset and stride, while other attributes are fixed values.
  pica200_reset(memory);
  struct Position {
    float x, y, z;
  };
  const Position positions[3]{{-.8f, -.8f, -.25f},
                              {.8f, -.8f, -.25f},
                              {0.f, .8f, -.25f}};
  struct PaddedColor {
    std::uint32_t padding;
    std::uint8_t rgba[4];
  };
  const PaddedColor colors[3]{{0xccccccccu, {255, 0, 0, 255}},
                              {0xccccccccu, {0, 255, 0, 255}},
                              {0xccccccccu, {0, 0, 255, 255}}};
  struct IntegerAttributes {
    std::int8_t byte_values[2];
    std::int16_t short_values[2];
  };
  const IntegerAttributes integer_attributes[3]{
      {{-7, 9}, {-300, 700}}, {{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};
  const std::uint8_t reordered_indices[3]{2, 1, 0};
  std::memcpy(memory.data() + 0x5000, positions, sizeof(positions));
  std::memcpy(memory.data() + 0x5100, colors, sizeof(colors));
  std::memcpy(memory.data() + 0x5180, integer_attributes,
              sizeof(integer_attributes));
  std::memcpy(memory.data() + 0x5200, reordered_indices,
              sizeof(reordered_indices));
  draw_cursor = 0x800;
  gpu_write(draw_cursor, 0x200, 0x5000u >> 3);
  gpu_write(draw_cursor, 0x201,
            0x00064fdbu); // f32x3, u8x4, fixed f32x4, s8x2, s16x2
  gpu_write(draw_cursor, 0x202, (4u << 28) | (1u << (16 + 2)));
  gpu_write(draw_cursor, 0x203, 0);
  gpu_write(draw_cursor, 0x204, 0);
  gpu_write(draw_cursor, 0x205, (1u << 28) | (12u << 16));
  gpu_write(draw_cursor, 0x206, 0x100);
  gpu_write(draw_cursor, 0x207, 0x1cu); // 4-byte padding, then attribute 1
  gpu_write(draw_cursor, 0x208, (2u << 28) | (8u << 16));
  gpu_write(draw_cursor, 0x209, 0x180);
  gpu_write(draw_cursor, 0x20a, 0x43u);
  gpu_write(draw_cursor, 0x20b,
            (2u << 28) | (sizeof(IntegerAttributes) << 16));
  gpu_write(draw_cursor, 0x227, 0x200);
  gpu_write(draw_cursor, 0x228, 3);
  gpu_write(draw_cursor, 0x22a, 0);
  gpu_write(draw_cursor, 0x232, 2);
  gpu_write(draw_cursor, 0x233, 0x3f000000u); // fixed {1, 0, 0, 1}
  gpu_write(draw_cursor, 0x234, 0);
  gpu_write(draw_cursor, 0x235, 0x003f0000u);
  gpu_write(draw_cursor, 0x11d, 0x4000u >> 3);
  gpu_write(draw_cursor, 0x11e, 8u | (7u << 12));
  gpu_write(draw_cursor, 0x117, 2);
  gpu_write(draw_cursor, 0x107, 0xf00);
  gpu_write(draw_cursor, 0x2cb, 0);
  gpu_write(draw_cursor, 0x2cc, 0x13u << 26); // MOV o0, v0
  gpu_write(draw_cursor, 0x2cc,
            (0x13u << 26) | (1u << 21) | (1u << 12)); // MOV o1, v1
  gpu_write(draw_cursor, 0x2cc,
            (0x13u << 26) | (2u << 21) | (2u << 12)); // MOV o2, v2
  gpu_write(draw_cursor, 0x2cc,
            (0x13u << 26) | (3u << 21) | (3u << 12)); // MOV o3, v3
  gpu_write(draw_cursor, 0x2cc,
            (0x13u << 26) | (4u << 21) | (4u << 12)); // MOV o4, v4
  gpu_write(draw_cursor, 0x2cc, 0x22u << 26);
  gpu_write(draw_cursor, 0x2d5, 0);
  gpu_write(draw_cursor, 0x2d6, 0x0000036fu);
  gpu_write(draw_cursor, 0x2ba, 0);
  gpu_write(draw_cursor, 0x2bb, 0x43210u);
  gpu_write(draw_cursor, 0x4f, 5);
  gpu_write(draw_cursor, 0x50, 0x03020100u);
  gpu_write(draw_cursor, 0x51, 0x0b0a0908u);
  gpu_write(draw_cursor, 0x52, 0x1f1f0d0cu);
  gpu_write(draw_cursor, 0x53, 0x1f1f1f1fu);
  gpu_write(draw_cursor, 0x54, 0x1f1f1f1fu);
  gpu_write(draw_cursor, 0x22f, 1);
  assert(pica200_decode_command_list(0x800, draw_cursor - 0x800));
  drawn = pica200_snapshot();
  assert(drawn.draw_calls == 1 && drawn.draws_rendered == 1);
  assert(drawn.last_input_position == PicaVec4({-.8f, -.8f, -.25f, 1.f}));
  assert(drawn.last_shader_outputs[1] ==
         PicaVec4({255.f, 0.f, 0.f, 255.f}));
  assert(drawn.last_shader_outputs[2] == PicaVec4({1.f, 0.f, 0.f, 1.f}));
  assert(drawn.last_shader_outputs[3] == PicaVec4({-7.f, 9.f, 0.f, 1.f}));
  assert(drawn.last_shader_outputs[4] ==
         PicaVec4({-300.f, 700.f, 0.f, 1.f}));
}
