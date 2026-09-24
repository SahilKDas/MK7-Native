#include <mk7/recomp/pica200.hpp>
#include <array>
#include <cassert>
#include <cstring>
#include <vector>

int main(){
 std::vector<std::byte> memory(0x8000);pica200_reset(memory);
 const std::uint32_t parameter0=0x11223344,header0=0x000f0041;
 const std::uint32_t parameter1=1,header1=0x000f022e;
 std::memcpy(memory.data()+0x100,&parameter0,4);std::memcpy(memory.data()+0x104,&header0,4);
 std::memcpy(memory.data()+0x108,&parameter1,4);std::memcpy(memory.data()+0x10c,&header1,4);
 assert(pica200_decode_command_list(0x100,16));auto state=pica200_snapshot();
 assert(state.command_lists==1&&state.register_writes==2&&state.draw_calls==1&&state.last_register==0x22e);
 assert(pica200_memory_fill(0x1000,0x1010,0xff336699,2));for(unsigned i=0;i<16;i+=4){std::uint32_t value{};std::memcpy(&value,memory.data()+0x1000+i,4);assert(value==0xff336699);}
 const std::uint32_t pixels[4]{0xff0000ff,0xff00ff00,0xffff0000,0xffffffff};std::memcpy(memory.data()+0x2000,pixels,sizeof(pixels));
 pica200_set_framebuffer(0,0x2000,8,0);std::array<std::uint32_t,1> output{};assert(pica200_present(0,output,1,1));assert(output[0]==pixels[0]);
 auto gpu_write=[&](unsigned& cursor,std::uint32_t reg,std::uint32_t value){const std::uint32_t header=reg|0x000f0000u;std::memcpy(memory.data()+cursor,&value,4);std::memcpy(memory.data()+cursor+4,&header,4);cursor+=8;};
 unsigned cursor=0x300;
 gpu_write(cursor,0x2cb,0);
 gpu_write(cursor,0x2cc,(0x13u<<26)|0u);
 gpu_write(cursor,0x2cc,0x22u<<26);
 gpu_write(cursor,0x2d5,0);
 gpu_write(cursor,0x2d6,0x0000036fu);
 gpu_write(cursor,0x2ba,0);
 assert(pica200_decode_command_list(0x300,cursor-0x300));
 const PicaVec4 vertex{1.25f,2.5f,-3.f,1.f};
 PicaVertexOutput shaded{};assert(pica200_run_vertex_shader(std::span<const PicaVec4>(&vertex,1),shaded));
 assert(shaded.registers[0]==vertex);
 std::array<PicaVec4,3> triangle_inputs{{{-0.8f,-0.8f,0.25f,1.f},{0.8f,-0.8f,0.25f,1.f},{0.f,0.8f,0.25f,1.f}}};
 std::array<std::span<const PicaVec4>,3> input_spans{};
 for(unsigned i=0;i<3;++i)input_spans[i]=std::span<const PicaVec4>(&triangle_inputs[i],1);
 std::array<std::uint32_t,64> triangle_pixels{};
 std::array<float,64> triangle_depth{};triangle_depth.fill(1.f);
 std::array<std::uint8_t,64> triangle_stencil{};
 const PicaRasterTarget triangle_target{triangle_pixels,triangle_depth,triangle_stencil,8,8};
 assert(pica200_shade_and_rasterize_triangle(input_spans,triangle_target,{},0,0,0));
 assert(triangle_pixels[4*8+4]!=0);
 assert(!pica200_shade_and_rasterize_triangle(input_spans,triangle_target,{},16,0,0));
 gpu_write(cursor,0x2cb,0);
 gpu_write(cursor,0x2cc,(0x13u<<26)|1u);
 gpu_write(cursor,0x2cc,0x22u<<26);
 gpu_write(cursor,0x2d5,1);
 gpu_write(cursor,0x2d6,0x00000aa8u);
 assert(pica200_decode_command_list(cursor-40,40));
 assert(pica200_run_vertex_shader(std::span<const PicaVec4>(&vertex,1),shaded));
 assert(shaded.registers[0][0]==vertex[1]&&shaded.registers[0][1]==0.f);
 assert(!pica200_decode_command_list(0x7ffc,16));
 gpu_write(cursor,0x2cb,0);gpu_write(cursor,0x2cc,0x10u<<26);
 assert(pica200_decode_command_list(cursor-16,16));
 assert(!pica200_run_vertex_shader(std::span<const PicaVec4>(&vertex,1),shaded));
}
