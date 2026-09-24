#include <mk7/recomp/pica200.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <bit>

namespace {
std::span<std::byte> memory;
std::array<std::uint32_t,0x300> registers{};
struct Framebuffer {std::uint32_t address{},stride{},format{};bool valid{};};
std::array<Framebuffer,2> framebuffers{};
Pica200Snapshot stats;
std::array<std::uint32_t,4096> shader_code{},shader_descriptors{};
std::array<PicaVec4,96> uniforms{};
std::uint32_t code_index{},descriptor_index{},uniform_index{},uniform_word_count{};
std::array<std::uint32_t,4> uniform_words{};
bool uniform_float32{};
float decode_f24(std::uint32_t value){const std::uint32_t sign=(value&0x800000u)<<8,exponent=(value>>16)&0x7fu,mantissa=value&0xffffu;if(exponent==0)return std::bit_cast<float>(sign|mantissa);if(exponent==0x7f)return std::bit_cast<float>(sign|0x7f800000u|(mantissa<<7));return std::bit_cast<float>(sign|((exponent+64u)<<23)|(mantissa<<7));}
template<class T> bool load(std::uint32_t address,T& value){if(std::uint64_t(address)+sizeof(T)>memory.size())return false;std::memcpy(&value,memory.data()+address,sizeof(T));return true;}
void write_register(std::uint32_t id,std::uint32_t value,std::uint32_t mask){
 if(id>=registers.size())return;
 auto& current=registers[id];for(unsigned byte=0;byte<4;++byte)if(mask&(1u<<byte)){const auto bits=0xffu<<(byte*8);current=(current&~bits)|(value&bits);}
 stats.last_register=id;stats.last_value=current;++stats.register_writes;
 if(id==0x2cb)code_index=current&4095u;
else if(id>=0x2cc&&id<=0x2d3){shader_code[code_index++&4095u]=current;}
else if(id==0x2d5)descriptor_index=current&4095u;
else if(id>=0x2d6&&id<=0x2dd){shader_descriptors[descriptor_index++&4095u]=current;}
else if(id==0x2c0){uniform_index=current&0x7fu;uniform_float32=(current>>31)!=0;uniform_word_count=0;}
else if(id>=0x2c1&&id<=0x2c8){uniform_words[uniform_word_count++&3u]=current;const auto needed=uniform_float32?4u:3u;if(uniform_word_count==needed){if(uniform_index<96){if(uniform_float32){for(unsigned i=0;i<4;++i)uniforms[uniform_index][3-i]=std::bit_cast<float>(uniform_words[i]);}else{const std::uint64_t low=std::uint64_t(uniform_words[0])|(std::uint64_t(uniform_words[1])<<32);const std::uint64_t high=uniform_words[2];const std::uint32_t components[4]{std::uint32_t((high>>16)&0xffffffu),std::uint32_t(((high&0xffffu)<<8)|(low>>56)),std::uint32_t((low>>32)&0xffffffu),std::uint32_t(low&0xffffffu)};for(unsigned i=0;i<4;++i)uniforms[uniform_index][i]=decode_f24(components[i]);}}++uniform_index;uniform_word_count=0;}}
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
void pica200_reset(std::span<std::byte> m) noexcept{memory=m;registers={};framebuffers={};stats={};shader_code={};shader_descriptors={};uniforms={};code_index=descriptor_index=uniform_index=uniform_word_count=0;uniform_words={};uniform_float32=false;}
bool pica200_decode_command_list(std::uint32_t address,std::uint32_t size) noexcept{
 if((size&7u)||std::uint64_t(address)+size>memory.size())return false;
 std::uint32_t cursor=address,end=address+size;++stats.command_lists;
 while(cursor<end){std::uint32_t parameter{},header{};if(!load(cursor,parameter)||!load(cursor+4,header))return false;cursor+=8;
  const auto id=header&0xffffu,mask=(header>>16)&15u,extra=(header>>20)&0xffu;const bool consecutive=bool(header>>31);
  write_register(id,parameter,mask);
  for(std::uint32_t i=0;i<extra;++i){if(std::uint64_t(cursor)+4>end||!load(cursor,parameter))return false;cursor+=4;write_register(consecutive?id+i+1:id,parameter,mask);}
  if((extra&1u)!=0){if(std::uint64_t(cursor)+4>end)return false;cursor+=4;}
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

namespace {
PicaVec4 shader_source(unsigned index,const std::array<PicaVec4,16>& inputs,const std::array<PicaVec4,16>& temporaries){
 if(index<16)return inputs[index];
 if(index<32)return temporaries[index-16];
 if(index<128)return uniforms[index-32];
 return {};
}
PicaVec4 shader_swizzle(PicaVec4 source,std::uint32_t selector,bool negate){
 PicaVec4 result{};for(unsigned i=0;i<4;++i){result[i]=source[(selector>>(6-2*i))&3u];if(negate)result[i]=-result[i];}return result;
}
}
bool pica200_run_vertex_shader(std::span<const PicaVec4> input,PicaVertexOutput& output) noexcept{
 std::array<PicaVec4,16> inputs{},temporaries{};output={};
 for(std::size_t i=0;i<std::min(input.size(),inputs.size());++i){const auto mapping=registers[i<8?0x2bb:0x2bc];const auto target=(mapping>>((i&7u)*4u))&15u;inputs[target]=input[i];}
 std::uint32_t pc=registers[0x2ba]&4095u;
 for(unsigned steps=0;steps<4096;++steps){
  if(pc>=shader_code.size())return false;const auto word=shader_code[pc++],opcode=word>>26;
  if(opcode==0x22)return true;
  if(opcode==0x21)continue;
  if(opcode!=0x00&&opcode!=0x01&&opcode!=0x02&&opcode!=0x08&&opcode!=0x0c&&opcode!=0x0d&&opcode!=0x13)return false;
  const auto descriptor=shader_descriptors[word&0x7fu],destination=(word>>21)&31u,source1=(word>>12)&127u,source2=(word>>7)&31u;
  const auto a=shader_swizzle(shader_source(source1,inputs,temporaries),(descriptor>>5)&255u,(descriptor&(1u<<4))!=0);
  const auto b=shader_swizzle(shader_source(source2,inputs,temporaries),(descriptor>>14)&255u,(descriptor&(1u<<13))!=0);
  PicaVec4 result{};
  if(opcode==0x01||opcode==0x02){float dot=0;for(unsigned i=0;i<(opcode==0x01?3u:4u);++i)dot+=a[i]*b[i];result.fill(dot);}
  else for(unsigned i=0;i<4;++i)switch(opcode){case 0x00:result[i]=a[i]+b[i];break;case 0x08:result[i]=a[i]*b[i];break;case 0x0c:result[i]=std::max(a[i],b[i]);break;case 0x0d:result[i]=std::min(a[i],b[i]);break;case 0x13:result[i]=a[i];break;default:break;}
  auto& dst=destination<16?output.registers[destination]:temporaries[destination-16];
  for(unsigned i=0;i<4;++i)if(descriptor&(1u<<(3-i)))dst[i]=result[i];
 }
 return false;
}bool pica200_shade_and_rasterize_triangle(const std::array<std::span<const PicaVec4>,3>& inputs, PicaRasterTarget target, const PicaRasterState& state, unsigned position_output, unsigned color_output, unsigned uv_output) noexcept{
 if(position_output>=16||color_output>=16||uv_output>=16)return false;
 std::array<PicaRasterVertex,3> vertices{};
 for(unsigned i=0;i<3;++i){
  PicaVertexOutput output{};
  if(!pica200_run_vertex_shader(inputs[i],output))return false;
  vertices[i].clip=output.registers[position_output];
  vertices[i].color=output.registers[color_output];
  vertices[i].uv={output.registers[uv_output][0],output.registers[uv_output][1]};
 }
 return pica_rasterize_triangle(vertices,target,state);
}