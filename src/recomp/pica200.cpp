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
struct Framebuffer {std::uint32_t address{},stride{},format{},width{},height{};bool rotate{},valid{};};
std::array<Framebuffer,2> framebuffers{};
Framebuffer render_target{};
Pica200Snapshot stats;
std::array<std::uint32_t,4096> shader_code{},shader_descriptors{};
std::array<PicaVec4,96> uniforms{};
std::uint32_t code_index{},descriptor_index{},uniform_index{},uniform_word_count{};
std::array<std::uint32_t,4> uniform_words{};
bool uniform_float32{};
unsigned command_depth{};
std::uint64_t last_presented_generation{};
constexpr std::array<std::uint32_t,6> tev_register_bases{0xc0u,0xc8u,0xd0u,0xd8u,0xf0u,0xf8u};
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
bool execute_draw(bool indexed) noexcept;
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
else if(id>=0x2c1&&id<=0x2c8){uniform_words[uniform_word_count++&3u]=current;const auto needed=uniform_float32?4u:3u;if(uniform_word_count==needed){if(uniform_index<96){if(uniform_float32){for(unsigned i=0;i<4;++i)uniforms[uniform_index][3-i]=std::bit_cast<float>(uniform_words[i]);}else{const std::uint32_t components[4]{uniform_words[2]>>8,((uniform_words[2]&0xffu)<<16)|(uniform_words[1]>>16),((uniform_words[1]&0xffffu)<<8)|(uniform_words[0]>>24),uniform_words[0]&0xffffffu};for(unsigned i=0;i<4;++i)uniforms[uniform_index][i]=decode_f24(components[i]);}}++uniform_index;uniform_word_count=0;}}
else if((id==0x23c||id==0x23d)&&current){const auto slot=id-0x23cu;const auto size=registers[0x238u+slot];const auto encoded=registers[0x23au+slot];if(size&&size<=0x20000u)decode_command_list(gpu_to_virtual(encoded<<3),size<<3,command_depth+1);}
if((id==0x22e||id==0x22f)&&current){++stats.draw_calls;if(execute_draw(id==0x22f))++stats.draws_rendered;else ++stats.draws_unsupported;}
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
std::size_t texture_byte_size(unsigned width,unsigned height,unsigned format){const auto pixels=std::uint64_t(width)*height;if(format==0)return pixels*4;if(format==1)return pixels*3;if(format<=6)return pixels*2;if(format<=9)return pixels;if(format<=11)return (pixels+1)/2;if(format<=13)return std::uint64_t((width+7)/8)*((height+7)/8)*4u*(format==12?8u:16u);return 0;}
unsigned morton8(unsigned x,unsigned y){unsigned value{};for(unsigned bit=0;bit<3;++bit){value|=((x>>bit)&1u)<<(bit*2);value|=((y>>bit)&1u)<<(bit*2+1);}return value;}
int sign3(unsigned value){return (value&4u)?int(value)-8:int(value);}
std::uint8_t expand4(unsigned value){return std::uint8_t((value<<4)|value);}
std::uint8_t expand5(unsigned value){return std::uint8_t((value<<3)|(value>>2));}
std::uint32_t decode_etc1_pixel(const std::byte* block,unsigned x,unsigned y,std::uint8_t alpha){
 std::uint64_t bits{};for(unsigned i=0;i<8;++i)bits=(bits<<8)|std::to_integer<std::uint8_t>(block[i]);
 const bool differential=(bits>>33)&1u,flip=(bits>>32)&1u;unsigned r[2]{},g[2]{},b[2]{};
 if(differential){const int br=(bits>>59)&31u,bg=(bits>>51)&31u,bb=(bits>>43)&31u;r[0]=expand5(br);g[0]=expand5(bg);b[0]=expand5(bb);r[1]=expand5(std::clamp(br+sign3((bits>>56)&7u),0,31));g[1]=expand5(std::clamp(bg+sign3((bits>>48)&7u),0,31));b[1]=expand5(std::clamp(bb+sign3((bits>>40)&7u),0,31));}
 else {r[0]=expand4((bits>>60)&15u);r[1]=expand4((bits>>56)&15u);g[0]=expand4((bits>>52)&15u);g[1]=expand4((bits>>48)&15u);b[0]=expand4((bits>>44)&15u);b[1]=expand4((bits>>40)&15u);}
 static constexpr int modifiers[8][4]{{2,8,-2,-8},{5,17,-5,-17},{9,29,-9,-29},{13,42,-13,-42},{18,60,-18,-60},{24,80,-24,-80},{33,106,-33,-106},{47,183,-47,-183}};
 const unsigned sub=flip?(y>=2):(x>=2),table=(bits>>(sub?34:37))&7u,index=x*4+y,selector=((bits>>index)&1u)|(((bits>>(index+16))&1u)<<1);const auto adjust=modifiers[table][selector];
 const auto clamp=[&](unsigned value){return std::uint32_t(std::clamp(int(value)+adjust,0,255));};return clamp(r[sub])|(clamp(g[sub])<<8)|(clamp(b[sub])<<16)|(std::uint32_t(alpha)<<24);
}
bool decode_texture0(std::vector<std::uint32_t>& output,unsigned width,unsigned height,unsigned format){
 const auto size=texture_byte_size(width,height,format);if(!size||width>2048||height>2048)return false;const auto address=gpu_to_virtual((registers[0x85]&0x0fffffffu)<<3);const auto data=map_memory(address,size);if(data.empty())return false;output.resize(std::size_t(width)*height);
 auto byte=[&](std::size_t offset){return std::to_integer<std::uint8_t>(data[offset]);};
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto texel=(std::uint64_t(y/8)*((width+7)/8)+x/8)*64u+morton8(x&7u,y&7u);std::uint32_t color{};
  if(format==0){const auto o=texel*4;color=byte(o)|(byte(o+1)<<8)|(byte(o+2)<<16)|(byte(o+3)<<24);}
  else if(format==1){const auto o=texel*3;color=byte(o)|(byte(o+1)<<8)|(byte(o+2)<<16)|0xff000000u;}
  else if(format>=2&&format<=4){std::uint16_t v{};std::memcpy(&v,data.data()+texel*2,2);if(format==2)color=((v>>11)&31)*255/31|(((v>>6)&31)*255/31<<8)|(((v>>1)&31)*255/31<<16)|((v&1)?0xff000000u:0);else if(format==3)color=((v>>11)&31)*255/31|(((v>>5)&63)*255/63<<8)|((v&31)*255/31<<16)|0xff000000u;else color=((v>>12)&15)*17|(((v>>8)&15)*17<<8)|(((v>>4)&15)*17<<16)|((v&15)*17<<24);}
  else if(format==5){const auto i=byte(texel*2),a=byte(texel*2+1);color=i|(i<<8)|(i<<16)|(a<<24);}
  else if(format==6){const auto r=byte(texel*2),g=byte(texel*2+1);color=r|(g<<8)|0xffff0000u;}
  else if(format==7){const auto i=byte(texel);color=i|(i<<8)|(i<<16)|0xff000000u;}
  else if(format==8)color=0x00ffffffu|(byte(texel)<<24);
  else if(format==9){const auto v=byte(texel);const auto i=(v>>4)*17,a=(v&15)*17;color=i|(i<<8)|(i<<16)|(a<<24);}
  else if(format<=11){const auto packed=byte(texel/2);const auto v=((texel&1)?packed>>4:packed&15)*17;color=format==10?(v|(v<<8)|(v<<16)|0xff000000u):(0x00ffffffu|(v<<24));}
  else {const std::uint64_t macro=std::uint64_t(y/8)*((width+7)/8)+x/8;const unsigned block_in_macro=((y&7u)/4u)*2u+((x&7u)/4u),block_size=format==12?8u:16u;std::uint64_t offset=(macro*4u+block_in_macro)*block_size;std::uint8_t alpha=255;if(format==13){const std::uint64_t alpha_bits=std::uint64_t(byte(offset))|(std::uint64_t(byte(offset+1))<<8)|(std::uint64_t(byte(offset+2))<<16)|(std::uint64_t(byte(offset+3))<<24)|(std::uint64_t(byte(offset+4))<<32)|(std::uint64_t(byte(offset+5))<<40)|(std::uint64_t(byte(offset+6))<<48)|(std::uint64_t(byte(offset+7))<<56);alpha=std::uint8_t(((alpha_bits>>((((y&3u)*4u)+(x&3u))*4u))&15u)*17u);offset+=8;}color=decode_etc1_pixel(data.data()+offset,x&3u,y&3u,alpha);}
  output[std::size_t(y)*width+x]=color;
 }
 return true;
}
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
void pica200_reset(std::span<std::byte> m) noexcept{memory=m;vram={};registers={};for(const auto base:tev_register_bases)registers[base]=0x0fff0fffu;framebuffers={};render_target={};stats={};last_presented_generation=0;shader_code={};shader_descriptors={};uniforms={};code_index=descriptor_index=uniform_index=uniform_word_count=0;uniform_words={};uniform_float32=false;}
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
void pica200_set_framebuffer(unsigned screen,std::uint32_t address,std::uint32_t stride,std::uint32_t format) noexcept{if(screen<2)framebuffers[screen]={address,stride,format,screen?320u:400u,240u,false,true};}
bool pica200_present(unsigned screen,std::span<std::uint32_t> out,unsigned width,unsigned height,std::uint64_t* generation) noexcept{
 if(screen>=2||out.size()<std::size_t(width)*height)return false;auto fb=framebuffers[screen];if(!fb.valid&&screen==0)fb=render_target;if(!fb.valid)return false;const auto bpp=bytes_per_pixel(fb.format),stride=fb.stride?fb.stride:fb.width*bpp;
 if(map_memory(fb.address,std::uint64_t(stride)*fb.height).empty())return false;
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){unsigned sx{},sy{};if(fb.rotate){sx=y*std::min(fb.width,240u)/std::max(height,1u);const auto logical_height=std::min(fb.height,400u);sy=logical_height-1u-x*logical_height/std::max(width,1u);}else{sx=x*fb.width/std::max(width,1u);sy=y*fb.height/std::max(height,1u);}out[std::size_t(y)*width+x]=decode_pixel(fb.address+sy*stride+sx*bpp,fb.format);}
 if(generation)*generation=stats.framebuffer_generation;
 return true;
}
void pica200_note_presented(std::uint64_t generation) noexcept{if(generation&&generation!=last_presented_generation){last_presented_generation=generation;++stats.presented_frames;}}
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
bool execute_draw(bool indexed) noexcept{
 // This intentionally narrow path accepts one float4 position stream, plain
 // triangle lists, an RGBA8 linear target, and no fragment side effects.
 if((registers[0x202]>>16)!=0 || registers[0x201]!=0x0fu ||
    registers[0x204]!=0 || registers[0x205]!=((1u<<28)|(16u<<16)) ||
    (registers[0x25e]&0x300u)!=0 || registers[0x229]!=0){++stats.rejected_state;return false;}
 const auto count=registers[0x228], first=registers[0x22a];
 stats.last_texture_config=registers[0x80];stats.last_texture_dimensions=registers[0x82];stats.last_texture_format=registers[0x8e];stats.last_texture_address=registers[0x85];stats.last_tev_source=registers[0xc0];stats.last_tev_combiner=registers[0xc2];if(registers[0x80]&1u)++stats.textured_draws;
 if(count<3||count>4096||count%3u){++stats.rejected_state;return false;}
 const auto width=registers[0x11e]&0x7ffu,height=((registers[0x11e]>>12)&0x3ffu)+1u;
 if(!width||width>1024||height>1024){++stats.rejected_bounds;return false;}
 const auto color_format=(registers[0x117]>>16)&7u,color_bpp=bytes_per_pixel(color_format);
 if(color_format>4){++stats.rejected_state;return false;}
 const std::uint32_t color_address=gpu_to_virtual((registers[0x11d]&0x0fffffffu)<<3);
 const std::uint32_t vertex_address=gpu_to_virtual((registers[0x200]&0x1fffffffu)<<3)+registers[0x203];
 const auto pixel_count=std::uint64_t(width)*height;
 auto color=map_memory(color_address,pixel_count*color_bpp);
 auto vertices=indexed?map_memory(vertex_address,16):map_memory(vertex_address,(std::uint64_t(first)+count)*16);
 const auto index_offset=registers[0x227]&0x7fffffffu,index_size=(registers[0x227]>>31)?2u:1u;
 auto indices=indexed?map_memory(vertex_address+index_offset,std::uint64_t(first+count)*index_size):std::span<std::byte>{};
 if(color.empty()||vertices.empty()||(indexed&&indices.empty())){++stats.rejected_bounds;return false;}
 std::vector<std::uint32_t> pixels(static_cast<std::size_t>(pixel_count));
 for(std::size_t i=0;i<pixels.size();++i)pixels[i]=decode_pixel(color_address+std::uint32_t(i*color_bpp),color_format);
 std::vector<float> depth(pixels.size(),1.f);
 std::vector<std::uint8_t> stencil(pixels.size());
 PicaRasterTarget target{pixels,depth,stencil,width,height};
 PicaRasterState state{};
 std::vector<std::uint32_t> texture_pixels;
 if(registers[0x80]&1u){const auto texture_width=(registers[0x82]>>16)&0x7ffu,texture_height=registers[0x82]&0x7ffu,texture_format=registers[0x8e]&15u;if(!decode_texture0(texture_pixels,texture_width,texture_height,texture_format)){++stats.rejected_state;return false;}const auto parameter=registers[0x83];state.texture_enable=true;state.texture={texture_pixels,texture_width,texture_height,bool(parameter&6u),((parameter>>12)&7u)==2u,((parameter>>8)&7u)==2u};}
 state.tev_enable=true;for(unsigned stage=0;stage<6;++stage){const auto base=tev_register_bases[stage];state.tev[stage]={registers[base],registers[base+1],registers[base+2],registers[base+3],registers[base+4]};stats.last_tev_sources[stage]=registers[base];stats.last_tev_operands[stage]=registers[base+1];stats.last_tev_combiners[stage]=registers[base+2];stats.last_tev_colors[stage]=registers[base+3];stats.last_tev_scales[stage]=registers[base+4];}
 std::array<PicaRasterVertex,3> triangle{};
 for(std::uint32_t i=0;i<count;++i){
  std::uint32_t vertex_index=first+i;if(indexed){if(index_size==2){std::uint16_t value{};std::memcpy(&value,indices.data()+std::uint64_t(first+i)*2,2);vertex_index=value;}else vertex_index=std::to_integer<std::uint8_t>(indices[first+i]);}
  PicaVec4 position{};
  const auto vertex_bytes=map_memory(vertex_address+std::uint64_t(vertex_index)*16,16);if(vertex_bytes.empty()){++stats.rejected_bounds;return false;}std::memcpy(position.data(),vertex_bytes.data(),16);
  stats.last_input_position=position;
  PicaVertexOutput output{};
  if(!pica200_run_vertex_shader(std::span<const PicaVec4>(&position,1),output)){++stats.rejected_shader;return false;}
  stats.last_shader_entry=registers[0x2ba]&4095u;
  auto& vertex=triangle[i%3];vertex.clip={0.f,0.f,0.f,1.f};vertex.color={1.f,1.f,1.f,1.f};
  const auto output_count=std::min(registers[0x4f]&7u,7u);
  stats.last_output_count=output_count;
  for(unsigned slot=0;slot<7;++slot){stats.last_shader_outputs[slot]=output.registers[slot];stats.last_output_mappings[slot]=registers[0x50u+slot];}
  if(!output_count)vertex.clip=output.registers[0];
  else for(unsigned slot=0;slot<output_count;++slot){const auto mapping=registers[0x50u+slot];for(unsigned component=0;component<4;++component){const auto semantic=(mapping>>(component*8))&31u;const auto value=output.registers[slot][component];if(semantic<4)vertex.clip[semantic]=value;else if(semantic>=8&&semantic<12)vertex.color[semantic-8]=value;else if(semantic==12)vertex.uv[0]=value;else if(semantic==13)vertex.uv[1]=value;}}
  stats.last_clip_position=vertex.clip;
  if(i%3==2&&!pica_rasterize_triangle(triangle,target,state)){++stats.rejected_raster;return false;}
 }
 std::uint64_t changed{};for(std::size_t i=0;i<pixels.size();++i)if(decode_pixel(color_address+std::uint32_t(i*color_bpp),color_format)!=pixels[i]){++changed;encode_pixel(color.subspan(i*color_bpp,color_bpp),pixels[i],color_format);}
 if(changed){++stats.pixel_producing_draws;stats.changed_pixels+=changed;++stats.framebuffer_generation;render_target={color_address,width*color_bpp,color_format,width,height,width<=256u&&height>=400u,true};}
 return true;
}
}
