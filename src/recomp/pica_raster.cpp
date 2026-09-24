#include <mk7/recomp/pica_raster.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
float clamp01(float value){return std::clamp(value,0.0f,1.0f);}
std::array<float,4> unpack(std::uint32_t color){return {float(color&255u)/255.f,float((color>>8)&255u)/255.f,float((color>>16)&255u)/255.f,float((color>>24)&255u)/255.f};}
std::uint32_t pack(const std::array<float,4>& color){std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(std::lround(clamp01(color[i])*255.f))<<(i*8);return value;}
bool compare(std::uint8_t function,float source,float destination){switch(function&7u){case 0:return false;case 1:return true;case 2:return source==destination;case 3:return source!=destination;case 4:return source<destination;case 5:return source<=destination;case 6:return source>destination;case 7:return source>=destination;}return false;}
std::uint8_t stencil_op(std::uint8_t operation,std::uint8_t old,std::uint8_t reference){switch(operation&7u){case 0:return old;case 1:return 0;case 2:return reference;case 3:return old==255?255:old+1;case 4:return old==0?0:old-1;case 5:return std::uint8_t(~old);case 6:return std::uint8_t(old+1);case 7:return std::uint8_t(old-1);}return old;}
float blend_factor(std::uint8_t factor,const std::array<float,4>& source,const std::array<float,4>& destination,unsigned component){switch(factor){case 0:return 0;case 1:return 1;case 2:return source[component];case 3:return 1-source[component];case 4:return destination[component];case 5:return 1-destination[component];case 6:return source[3];case 7:return 1-source[3];case 8:return destination[3];case 9:return 1-destination[3];default:return 0;}}
std::array<float,4> blend(const std::array<float,4>& source,const std::array<float,4>& destination,const PicaRasterState& state){
 if(!state.blend)return source;
 std::array<float,4> result{};
 for(unsigned i=0;i<4;++i){const float a=source[i]*blend_factor(state.blend_source,source,destination,i),b=destination[i]*blend_factor(state.blend_destination,source,destination,i);switch(state.blend_equation){case 0:result[i]=a+b;break;case 1:result[i]=a-b;break;case 2:result[i]=b-a;break;case 3:result[i]=std::min(source[i],destination[i]);break;case 4:result[i]=std::max(source[i],destination[i]);break;default:result[i]=source[i];break;}}
 return result;
}
int address_texel(int coordinate,unsigned size,bool repeat){if(!size)return 0;if(repeat){const int n=int(size);return ((coordinate%n)+n)%n;}return std::clamp(coordinate,0,int(size)-1);}
std::array<float,4> sample(const PicaRasterTexture& texture,float u,float v){
 if(!texture.width||!texture.height||texture.rgba.size()<std::size_t(texture.width)*texture.height)return {1,1,1,1};
 const float x=u*float(texture.width)-0.5f,y=v*float(texture.height)-0.5f;
 auto texel=[&](int tx,int ty){const auto ix=address_texel(tx,texture.width,texture.repeat_u),iy=address_texel(ty,texture.height,texture.repeat_v);return unpack(texture.rgba[std::size_t(iy)*texture.width+ix]);};
 if(!texture.linear)return texel(int(std::floor(x+0.5f)),int(std::floor(y+0.5f)));
 const int x0=int(std::floor(x)),y0=int(std::floor(y));const float fx=x-x0,fy=y-y0;const auto a=texel(x0,y0),b=texel(x0+1,y0),c=texel(x0,y0+1),d=texel(x0+1,y0+1);std::array<float,4> result{};for(unsigned i=0;i<4;++i)result[i]=(a[i]*(1-fx)+b[i]*fx)*(1-fy)+(c[i]*(1-fx)+d[i]*fx)*fy;return result;
}
float edge(float ax,float ay,float bx,float by,float px,float py){return (px-ax)*(by-ay)-(py-ay)*(bx-ax);}
}
bool pica_rasterize_triangle(const std::array<PicaRasterVertex,3>& vertices,PicaRasterTarget target,const PicaRasterState& state) noexcept{
 const auto pixels=std::size_t(target.width)*target.height;
 if(!target.width||!target.height||target.rgba.size()<pixels||target.depth.size()<pixels||target.stencil.size()<pixels)return false;
 if(state.texture_enable&&(!state.texture.width||!state.texture.height||state.texture.rgba.size()<std::size_t(state.texture.width)*state.texture.height))return false;
 if(state.blend&&(state.blend_equation>4||state.blend_source>9||state.blend_destination>9))return false;
 float x[3],y[3],inverse_w[3];
 for(unsigned i=0;i<3;++i){const auto& clip=vertices[i].clip;if(!(clip[3]>0)||!std::isfinite(clip[3]))return false;inverse_w[i]=1.f/clip[3];const float nx=clip[0]*inverse_w[i],ny=clip[1]*inverse_w[i];if(!std::isfinite(nx)||!std::isfinite(ny))return false;x[i]=(nx*.5f+.5f)*target.width;y[i]=(1.f-(ny*.5f+.5f))*target.height;}
 const float area=edge(x[0],y[0],x[1],y[1],x[2],y[2]);if(area==0||!std::isfinite(area))return false;
 const int min_x=std::max(0,int(std::floor(std::min({x[0],x[1],x[2]})))),max_x=std::min(int(target.width)-1,int(std::ceil(std::max({x[0],x[1],x[2]}))));
 const int min_y=std::max(0,int(std::floor(std::min({y[0],y[1],y[2]})))),max_y=std::min(int(target.height)-1,int(std::ceil(std::max({y[0],y[1],y[2]}))));
 for(int py=min_y;py<=max_y;++py)for(int px=min_x;px<=max_x;++px){
  const float cx=px+.5f,cy=py+.5f;
  const float b0=edge(x[1],y[1],x[2],y[2],cx,cy)/area,b1=edge(x[2],y[2],x[0],y[0],cx,cy)/area,b2=1.f-b0-b1;
  if(b0<0||b1<0||b2<0)continue;
  const float denominator=b0*inverse_w[0]+b1*inverse_w[1]+b2*inverse_w[2];if(!(denominator>0))continue;
  const float weights[3]{b0*inverse_w[0]/denominator,b1*inverse_w[1]/denominator,b2*inverse_w[2]/denominator};
  const auto offset=std::size_t(py)*target.width+unsigned(px);
  const float z=clamp01(weights[0]*vertices[0].clip[2]+weights[1]*vertices[1].clip[2]+weights[2]*vertices[2].clip[2]);
  auto update_stencil=[&](std::uint8_t operation){const auto old=target.stencil[offset],next=stencil_op(operation,old,state.stencil_reference);target.stencil[offset]=std::uint8_t((old&~state.stencil_write_mask)|(next&state.stencil_write_mask));};
  if(state.stencil_test&&!compare(state.stencil_compare,float(state.stencil_reference&state.stencil_mask),float(target.stencil[offset]&state.stencil_mask))){update_stencil(state.stencil_fail);continue;}
  if(state.depth_test&&!compare(state.depth_compare,z,target.depth[offset])){if(state.stencil_test)update_stencil(state.stencil_depth_fail);continue;}
  if(state.stencil_test)update_stencil(state.stencil_pass);
  std::array<float,4> color{};for(unsigned i=0;i<4;++i)for(unsigned v=0;v<3;++v)color[i]+=weights[v]*vertices[v].color[i];
  if(state.texture_enable){float u=0,v=0;for(unsigned i=0;i<3;++i){u+=weights[i]*vertices[i].uv[0];v+=weights[i]*vertices[i].uv[1];}const auto texel=sample(state.texture,u,v);for(unsigned i=0;i<4;++i)color[i]*=texel[i];}
  const auto old=unpack(target.rgba[offset]),mixed=blend(color,old,state);auto packed=unpack(target.rgba[offset]);for(unsigned i=0;i<4;++i)if(state.color_mask&(1u<<i))packed[i]=mixed[i];target.rgba[offset]=pack(packed);
  if(state.depth_write)target.depth[offset]=z;
 }
 return true;
}
