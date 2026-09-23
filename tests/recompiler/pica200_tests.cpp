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
}
