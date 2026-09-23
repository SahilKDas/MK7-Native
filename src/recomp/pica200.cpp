#include <mk7/recomp/pica200.hpp>
#include <algorithm>
#include <array>
#include <cstring>

namespace {
std::span<std::byte> memory;
std::array<std::uint32_t,0x300> registers{};
struct Framebuffer {std::uint32_t address{},stride{},format{};bool valid{};};
std::array<Framebuffer,2> framebuffers{};
Pica200Snapshot stats;
template<class T> bool load(std::uint32_t address,T& value){if(std::uint64_t(address)+sizeof(T)>memory.size())return false;std::memcpy(&value,memory.data()+address,sizeof(T));return true;}
void write_register(std::uint32_t id,std::uint32_t value,std::uint32_t mask){
 if(id>=registers.size())return;
 auto& current=registers[id];for(unsigned byte=0;byte<4;++byte)if(mask&(1u<<byte)){const auto bits=0xffu<<(byte*8);current=(current&~bits)|(value&bits);}
 stats.last_register=id;stats.last_value=current;++stats.register_writes;
 if(id==0x22e||id==0x22f)++stats.draw_calls;
}
std::uint32_t decode_pixel(std::uint32_t address,std::uint32_t format){
 auto byte=[&](unsigned n){return std::uint32_t(static_cast<std::uint8_t>(memory[address+n]));};
 switch(format&7u){
 case 0:return byte(0)|(byte(1)<<8)|(byte(2)<<16)|(byte(3)<<24);
 case 1:return 0xff000000u|(byte(0)<<16)|(byte(1)<<8)|byte(2);
 case 2:{std::uint16_t v{};std::memcpy(&v,memory.data()+address,2);return 0xff000000u|(((v>>11)&31)*255/31<<16)|(((v>>5)&63)*255/63<<8)|((v&31)*255/31);}
 case 3:{std::uint16_t v{};std::memcpy(&v,memory.data()+address,2);return ((v&1)?0xff000000u:0)|(((v>>11)&31)*255/31<<16)|(((v>>6)&31)*255/31<<8)|(((v>>1)&31)*255/31);}
 case 4:{std::uint16_t v{};std::memcpy(&v,memory.data()+address,2);return ((v&15)*17u<<24)|(((v>>12)&15)*17u<<16)|(((v>>8)&15)*17u<<8)|((v>>4)&15)*17u;}
 default:return 0xff000000u;
 }
}
unsigned bytes_per_pixel(std::uint32_t format){return (format&7u)==0?4u:(format&7u)==1?3u:2u;}
}
void pica200_reset(std::span<std::byte> m) noexcept{memory=m;registers={};framebuffers={};stats={};}
bool pica200_decode_command_list(std::uint32_t address,std::uint32_t size) noexcept{
 if((size&7u)||std::uint64_t(address)+size>memory.size())return false;
 std::uint32_t cursor=address,end=address+size;++stats.command_lists;
 while(cursor<end){std::uint32_t parameter{},header{};if(!load(cursor,parameter)||!load(cursor+4,header))return false;cursor+=8;
  const auto id=header&0xffffu,mask=(header>>16)&15u,extra=(header>>20)&0x7ffu;const bool consecutive=bool(header>>31);
  write_register(id,parameter,mask);
  for(std::uint32_t i=0;i<extra;++i){if(!load(cursor,parameter))return false;cursor+=4;write_register(consecutive?id+i+1:id,parameter,mask);}
  if((extra&1u)!=0&&cursor<end)cursor+=4;
 }
 return cursor==end;
}
bool pica200_memory_fill(std::uint32_t start,std::uint32_t end,std::uint32_t value,std::uint16_t control) noexcept{
 if(start>=end||end>memory.size())return false;const auto mode=control&3u;
 if(mode==0){for(auto p=start;p<end;++p)memory[p]=std::byte(value&0xffu);}
 else if(mode==1){for(auto p=start;p+1<end;p+=2){const auto v=std::uint16_t(value);std::memcpy(memory.data()+p,&v,2);}}
 else {for(auto p=start;p+3<end;p+=4)std::memcpy(memory.data()+p,&value,4);}
 ++stats.memory_fills;return true;
}
void pica200_set_framebuffer(unsigned screen,std::uint32_t address,std::uint32_t stride,std::uint32_t format) noexcept{if(screen<2)framebuffers[screen]={address,stride,format,true};}
bool pica200_present(unsigned screen,std::span<std::uint32_t> out,unsigned width,unsigned height) noexcept{
 if(screen>=2||out.size()<std::size_t(width)*height||!framebuffers[screen].valid)return false;const auto fb=framebuffers[screen];const unsigned source_width=screen?320u:400u,source_height=240u,bpp=bytes_per_pixel(fb.format),stride=fb.stride?fb.stride:source_width*bpp;
 if(std::uint64_t(fb.address)+std::uint64_t(stride)*source_height>memory.size())return false;
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto sx=x*source_width/std::max(width,1u),sy=y*source_height/std::max(height,1u);out[std::size_t(y)*width+x]=decode_pixel(fb.address+sy*stride+sx*bpp,fb.format);}
 return true;
}
Pica200Snapshot pica200_snapshot() noexcept{return stats;}
