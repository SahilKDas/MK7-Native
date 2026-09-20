#include <mk7/host/layout.hpp>
#include <algorithm>
namespace mk7::host {
std::array<ScreenRect,2> compute_screen_layout(int width,int height) noexcept {
 constexpr float gap=24.f;float scale=std::min(float(width)/400.f,float(height-gap)/480.f);scale=std::max(scale,0.1f);
 int tw=int(400*scale),th=int(240*scale),bw=int(320*scale),bh=int(240*scale),total=th+int(gap)+bh;
 return {{{(width-tw)/2,(height-total)/2,tw,th},{(width-bw)/2,(height-total)/2+th+int(gap),bw,bh}}};
}
}
