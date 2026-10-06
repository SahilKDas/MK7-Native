#include <mk7/recomp/pica200.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <bit>
#include <vector>

namespace {
std::span<std::byte> memory;
constexpr std::uint32_t vram_base=0x1f000000u,vram_size=0x00600000u;
std::array<std::byte,vram_size> vram{};
std::array<std::uint32_t,0x300> registers{};
struct Framebuffer {std::uint32_t address{},stride{},format{};bool valid{};};
std::array<Framebuffer,2> framebuffers{};
Pica200Snapshot stats;
std::array<std::uint32_t,4096> shader_code{},shader_descriptors{};
std::array<PicaVec4,96> uniforms{};
std::uint32_t code_index{},descriptor_index{},uniform_index{},uniform_word_count{};
std::array<std::uint32_t,4> uniform_words{};
bool uniform_float32{};
unsigned command_depth{};
float decode_f24(std::uint32_t value){const std::uint32_t sign=(value&0x800000u)<<8,exponent=(value>>16)&0x7fu,mantissa=value&0xffffu;if(exponent==0)return std::bit_cast<float>(sign|mantissa);if(exponent==0x7f)return std::bit_cast<float>(sign|0x7f800000u|(mantissa<<7));return std::bit_cast<float>(sign|((exponent+64u)<<23)|(mantissa<<7));}
std::span<std::byte> map_memory(std::uint32_t address,std::size_t size){
 if(std::uint64_t(address)+size<=memory.size())return memory.subspan(address,size);
 if(address>=vram_base&&std::uint64_t(address-vram_base)+size<=vram.size())return std::span<std::byte>{vram}.subspan(address-vram_base,size);
 return {};
}
std::uint32_t gpu_to_virtual(std::uint32_t address){
 if(address>=0x18000000u&&address<0x18600000u)return address+0x07000000u;
 if(address>=0x20000000u&&address<0x28000000u)return address-0x0c000000u;
 return address;
}
template<class T> bool load(std::uint32_t address,T& value){const auto mapped=map_memory(address,sizeof(T));if(mapped.empty())return false;std::memcpy(&value,mapped.data(),sizeof(T));return true;}
bool execute_draw_arrays() noexcept;
bool decode_command_list(std::uint32_t address,std::uint32_t size,unsigned depth) noexcept;
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
else if((id==0x23c||id==0x23d)&&current){const auto slot=id-0x23cu;const auto size=registers[0x238u+slot];const auto encoded=registers[0x23au+slot];if(size&&size<=0x20000u)decode_command_list(gpu_to_virtual(encoded<<3),size<<3,command_depth+1);}
if((id==0x22e||id==0x22f)&&current){++stats.draw_calls;if(id==0x22e&&execute_draw_arrays())++stats.draws_rendered;else ++stats.draws_unsupported;}
}
std::uint32_t decode_pixel(std::uint32_t address,std::uint32_t format){
 const auto mapped=map_memory(address,4);if(mapped.empty())return 0xff000000u;
 auto byte=[&](unsigned n){return std::uint32_t(static_cast<std::uint8_t>(mapped[n]));};
 switch(format&7u){
 case 0:return byte(0)|(byte(1)<<8)|(byte(2)<<16)|(byte(3)<<24);
 case 1:return 0xff000000u|(byte(0)<<16)|(byte(1)<<8)|byte(2);
 case 2:{std::uint16_t v{};std::memcpy(&v,mapped.data(),2);return 0xff000000u|(((v>>11)&31)*255/31<<16)|(((v>>5)&63)*255/63<<8)|((v&31)*255/31);}
 case 3:{std::uint16_t v{};std::memcpy(&v,mapped.data(),2);return ((v&1)?0xff000000u:0)|(((v>>11)&31)*255/31<<16)|(((v>>6)&31)*255/31<<8)|(((v>>1)&31)*255/31);}
 case 4:{std::uint16_t v{};std::memcpy(&v,mapped.data(),2);return ((v&15)*17u<<24)|(((v>>12)&15)*17u<<16)|(((v>>8)&15)*17u<<8)|((v>>4)&15)*17u;}
 default:return 0xff000000u;
 }
}
unsigned bytes_per_pixel(std::uint32_t format){return (format&7u)==0?4u:(format&7u)==1?3u:2u;}
void encode_pixel(std::span<std::byte> mapped,std::uint32_t rgba,std::uint32_t format){
 const auto r=rgba&255u,g=(rgba>>8)&255u,b=(rgba>>16)&255u,a=rgba>>24;
 if((format&7u)==0){std::memcpy(mapped.data(),&rgba,4);return;}
 if((format&7u)==1){mapped[0]=std::byte(b);mapped[1]=std::byte(g);mapped[2]=std::byte(r);return;}
 std::uint16_t value{};
 if((format&7u)==2)value=std::uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3));
 else if((format&7u)==3)value=std::uint16_t((r>>3)<<11|(g>>3)<<6|(b>>3)<<1|(a>=128));
 else value=std::uint16_t((r>>4)<<12|(g>>4)<<8|(b>>4)<<4|(a>>4));
 std::memcpy(mapped.data(),&value,2);
}
}
void pica200_reset(std::span<std::byte> m) noexcept{memory=m;vram={};registers={};framebuffers={};stats={};shader_code={};shader_descriptors={};uniforms={};code_index=descriptor_index=uniform_index=uniform_word_count=0;uniform_words={};uniform_float32=false;}
bool pica200_decode_command_list(std::uint32_t address,std::uint32_t size) noexcept{
 return decode_command_list(address,size,0);
}
namespace {
bool decode_command_list(std::uint32_t address,std::uint32_t size,unsigned depth) noexcept{
 if(depth>8||(size&7u)||map_memory(address,size).empty())return false;
 struct DepthGuard {unsigned previous;DepthGuard(unsigned value):previous(command_depth){command_depth=value;}~DepthGuard(){command_depth=previous;}} guard(depth);
 std::uint32_t cursor=address,end=address+size;++stats.command_lists;
 while(cursor<end){std::uint32_t parameter{},header{};if(!load(cursor,parameter)||!load(cursor+4,header))return false;cursor+=8;
  const auto id=header&0xffffu,mask=(header>>16)&15u,extra=(header>>20)&0xffu;const bool consecutive=bool(header>>31);
  write_register(id,parameter,mask);
  for(std::uint32_t i=0;i<extra;++i){if(std::uint64_t(cursor)+4>end||!load(cursor,parameter))return false;cursor+=4;write_register(consecutive?id+i+1:id,parameter,mask);}
  if((extra&1u)!=0){if(std::uint64_t(cursor)+4>end)return false;cursor+=4;}
 }
 return cursor==end;
}
}
bool pica200_memory_fill(std::uint32_t start,std::uint32_t end,std::uint32_t value,std::uint16_t control) noexcept{
 if(start>=end)return false;auto mapped=map_memory(start,end-start);if(mapped.empty())return false;const auto mode=control&3u;
 if(mode==0){std::fill(mapped.begin(),mapped.end(),std::byte(value&0xffu));}
 else if(mode==1){for(std::size_t p=0;p+1<mapped.size();p+=2){const auto v=std::uint16_t(value);std::memcpy(mapped.data()+p,&v,2);}}
 else {for(std::size_t p=0;p+3<mapped.size();p+=4)std::memcpy(mapped.data()+p,&value,4);}
 ++stats.memory_fills;return true;
}
bool pica200_transfer(std::uint32_t source,std::uint32_t destination,std::uint32_t size) noexcept{
 auto src=map_memory(source,size),dst=map_memory(destination,size);if(src.empty()||dst.empty())return false;
 std::memmove(dst.data(),src.data(),size);return true;
}
bool pica200_display_transfer(std::uint32_t source,std::uint32_t destination,std::uint32_t input_dimensions,std::uint32_t output_dimensions,std::uint32_t flags) noexcept{
 const auto input_width=input_dimensions>>16,input_height=input_dimensions&0xffffu;
 const auto output_width=output_dimensions>>16,output_height=output_dimensions&0xffffu;
 if(!input_width||!input_height||!output_width||!output_height)return false;
 const auto input_format=(flags>>8)&7u,output_format=(flags>>12)&7u;
 const auto input_bpp=bytes_per_pixel(input_format),output_bpp=bytes_per_pixel(output_format);
 if(input_width!=output_width||input_height!=output_height||input_bpp!=output_bpp)return false;
 const auto size=std::uint64_t(input_width)*input_height*input_bpp;if(size>0xffffffffu)return false;
 return pica200_transfer(source,destination,std::uint32_t(size));
}
void pica200_set_framebuffer(unsigned screen,std::uint32_t address,std::uint32_t stride,std::uint32_t format) noexcept{if(screen<2)framebuffers[screen]={address,stride,format,true};}
bool pica200_present(unsigned screen,std::span<std::uint32_t> out,unsigned width,unsigned height) noexcept{
 if(screen>=2||out.size()<std::size_t(width)*height||!framebuffers[screen].valid)return false;const auto fb=framebuffers[screen];const unsigned source_width=screen?320u:400u,source_height=240u,bpp=bytes_per_pixel(fb.format),stride=fb.stride?fb.stride:source_width*bpp;
 if(map_memory(fb.address,std::uint64_t(stride)*source_height).empty())return false;
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
 std::array<int,3> address{};
 std::array<bool,2> compare{};std::uint32_t skip_at{},skip_to{},loop_start{},loop_after{},loop_remaining{};int loop_increment{};
 for(std::size_t i=0;i<std::min(input.size(),inputs.size());++i){const auto mapping=registers[i<8?0x2bb:0x2bc];const auto target=(mapping>>((i&7u)*4u))&15u;inputs[target]=input[i];}
 std::uint32_t pc=registers[0x2ba]&4095u;
 for(unsigned steps=0;steps<4096;++steps){
  if(skip_at&&pc==skip_at){pc=skip_to;skip_at=skip_to=0;}
  if(loop_after&&pc==loop_after){if(loop_remaining>1){--loop_remaining;address[2]+=loop_increment;pc=loop_start;}else{loop_after=loop_remaining=0;}}
  if(pc>=shader_code.size())return false;const auto word=shader_code[pc++],opcode=word>>26;
  if(opcode==0x22)return true;
  if(opcode==0x21)continue;
  auto condition=[&]{const auto operation=(word>>22)&3u;const bool x=compare[0]==bool((word>>25)&1u),y=compare[1]==bool((word>>24)&1u);return operation==0?x||y:operation==1?x&&y:operation==2?x: y;};
  if(opcode==0x28){const auto destination=(word>>10)&0xfffu,count=word&0xffu;if(condition()){skip_at=destination;skip_to=destination+count;}else pc=destination;continue;}
  if(opcode==0x29){const auto id=(word>>22)&3u,value=registers[0x2b1u+id];loop_remaining=(value&0xffu)+1u;address[2]=std::int8_t((value>>8)&0xffu);loop_increment=std::int8_t((value>>16)&0xffu);loop_start=pc;loop_after=((word>>10)&0xfffu)+1u;continue;}
  if(opcode==0x2c){if(condition())pc=(word>>10)&0xfffu;continue;}
  if(opcode==0x2d){const auto id=(word>>22)&15u;bool take=((registers[0x2b0]>>id)&1u)!=0;if(word&1u)take=!take;if(take)pc=(word>>10)&0xfffu;continue;}
  if((opcode>0x13||(opcode>=0x10&&opcode<=0x11))&&opcode!=0x2e&&opcode!=0x2f)return false;
  const auto descriptor=shader_descriptors[word&0x7fu],destination=(word>>21)&31u,source1=(word>>12)&127u,source2=(word>>7)&31u;
  auto indexed_source1=source1;const auto index=(word>>19)&3u;
  if(index&&source1>=32){const auto offset=address[index-1];indexed_source1=std::uint32_t((int(source1)+offset)&127);}
  const auto a=shader_swizzle(shader_source(indexed_source1,inputs,temporaries),(descriptor>>5)&255u,(descriptor&(1u<<4))!=0);
  const auto b=shader_swizzle(shader_source(source2,inputs,temporaries),(descriptor>>14)&255u,(descriptor&(1u<<13))!=0);
  if(opcode==0x2e||opcode==0x2f){auto test=[](float x,float y,unsigned operation){switch(operation){case 0:return x==y;case 1:return x!=y;case 2:return x<y;case 3:return x<=y;case 4:return x>y;case 5:return x>=y;default:return false;}};compare[0]=test(a[0],b[0],(word>>24)&7u);compare[1]=test(a[1],b[1],(word>>21)&7u);continue;}
  if(opcode==0x12){for(unsigned i=0;i<2;++i)if(descriptor&(1u<<(3-i)))address[i]=int(a[i]);continue;}
  PicaVec4 result{};
  if(opcode==0x01||opcode==0x02){float dot=0;for(unsigned i=0;i<(opcode==0x01?3u:4u);++i)dot+=a[i]*b[i];result.fill(dot);}
  else if(opcode==0x03){const float dot=a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+b[3];result.fill(dot);}
  else if(opcode==0x04)result={1.f,a[1]*b[1],a[2],b[3]};
  else if(opcode==0x05)result.fill(std::exp2(a[0]));
  else if(opcode==0x06)result.fill(std::log2(a[0]));
  else if(opcode==0x07)result={std::max(a[0],0.f),std::clamp(a[1],-127.9961f,127.9961f),0.f,std::max(a[3],0.f)};
  else if(opcode==0x0e)result.fill(1.f/a[0]);
  else if(opcode==0x0f)result.fill(1.f/std::sqrt(a[0]));
  else for(unsigned i=0;i<4;++i)switch(opcode){case 0x00:result[i]=a[i]+b[i];break;case 0x08:result[i]=a[i]*b[i];break;case 0x09:result[i]=a[i]>=b[i]?1.f:0.f;break;case 0x0a:result[i]=a[i]<b[i]?1.f:0.f;break;case 0x0b:result[i]=std::floor(a[i]);break;case 0x0c:result[i]=std::max(a[i],b[i]);break;case 0x0d:result[i]=std::min(a[i],b[i]);break;case 0x13:result[i]=a[i];break;default:break;}
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
}namespace {
bool execute_draw_arrays() noexcept{
 // This intentionally narrow path accepts one float4 position stream, plain
 // triangle lists, an RGBA8 linear target, and no fragment side effects.
 if((registers[0x202]>>16)!=0 || registers[0x201]!=0x0fu ||
    registers[0x204]!=0 || registers[0x205]!=((1u<<28)|(16u<<16)) ||
    (registers[0x25e]&0x300u)!=0 || registers[0x229]!=0)return false;
 const auto count=registers[0x228], first=registers[0x22a];
 if(count<3||count>4096||count%3u)return false;
 const auto width=registers[0x11e]&0x7ffu,height=((registers[0x11e]>>12)&0x3ffu)+1u;
 if(!width||width>1024||height>1024)return false;
 const auto color_format=(registers[0x117]>>16)&7u,color_bpp=bytes_per_pixel(color_format);
 if(color_format>4)return false;
 const std::uint32_t color_address=gpu_to_virtual((registers[0x11d]&0x0fffffffu)<<3);
 const std::uint32_t vertex_address=gpu_to_virtual((registers[0x200]&0x1fffffffu)<<3)+registers[0x203];
 const auto pixel_count=std::uint64_t(width)*height;
 auto color=map_memory(color_address,pixel_count*color_bpp);
 auto vertices=map_memory(vertex_address,(std::uint64_t(first)+count)*16);
 if(color.empty()||vertices.empty())return false;
 std::vector<std::uint32_t> pixels(static_cast<std::size_t>(pixel_count));
 for(std::size_t i=0;i<pixels.size();++i)pixels[i]=decode_pixel(color_address+std::uint32_t(i*color_bpp),color_format);
 std::vector<float> depth(pixels.size(),1.f);
 std::vector<std::uint8_t> stencil(pixels.size());
 PicaRasterTarget target{pixels,depth,stencil,width,height};
 PicaRasterState state{};
 std::array<PicaRasterVertex,3> triangle{};
 for(std::uint32_t i=0;i<count;++i){
  PicaVec4 position{};
  std::memcpy(position.data(),vertices.data()+(std::uint64_t(first)+i)*16,16);
  PicaVertexOutput output{};
  if(!pica200_run_vertex_shader(std::span<const PicaVec4>(&position,1),output))return false;
  auto& vertex=triangle[i%3];vertex.clip={0.f,0.f,0.f,1.f};vertex.color={1.f,1.f,1.f,1.f};
  const auto output_count=std::min(registers[0x4f]&7u,7u);
  if(!output_count)vertex.clip=output.registers[0];
  else for(unsigned slot=0;slot<output_count;++slot){const auto mapping=registers[0x50u+slot];for(unsigned component=0;component<4;++component){const auto semantic=(mapping>>(component*8))&31u;const auto value=output.registers[slot][component];if(semantic<4)vertex.clip[semantic]=value;else if(semantic>=8&&semantic<12)vertex.color[semantic-8]=value;else if(semantic==12)vertex.uv[0]=value;else if(semantic==13)vertex.uv[1]=value;}}
  // MK7's early 2D scene producer uses an orthographic path whose final W
  // normalization is performed by PICA fixed-function state not yet modeled.
  if(vertex.clip[3]==0.f&&std::isfinite(vertex.clip[0])&&std::isfinite(vertex.clip[1]))vertex.clip[3]=1.f;
  if(i%3==2&&!pica_rasterize_triangle(triangle,target,state))return false;
 }
 for(std::size_t i=0;i<pixels.size();++i)encode_pixel(color.subspan(i*color_bpp,color_bpp),pixels[i],color_format);
 return true;
}
}
