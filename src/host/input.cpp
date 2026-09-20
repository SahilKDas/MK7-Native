#include <mk7/host/input.hpp>
#include <algorithm>
namespace mk7::host {
void update_key(InputState& s,Key k,bool down) noexcept {
 auto set=[&](Button b){auto bit=static_cast<std::uint32_t>(b);if(down)s.buttons|=bit;else s.buttons&=~bit;};
 switch(k){case Key::a:set(Button::a);break;case Key::b:set(Button::b);break;case Key::x:set(Button::x);break;case Key::y:set(Button::y);break;case Key::l:set(Button::l);break;case Key::r:set(Button::r);break;case Key::start:set(Button::start);break;case Key::select:set(Button::select);break;case Key::up:set(Button::dpad_up);break;case Key::down:set(Button::dpad_down);break;case Key::left:set(Button::dpad_left);break;case Key::right:set(Button::dpad_right);break;default:break;}
 auto axis=[&](Key neg,Key pos,float& v){if(k==neg)v=down?-1.f:(v<0?0.f:v);if(k==pos)v=down?1.f:(v>0?0.f:v);};
 axis(Key::circle_left,Key::circle_right,s.circle_x);axis(Key::circle_down,Key::circle_up,s.circle_y);
 s.circle_x=std::clamp(s.circle_x,-1.f,1.f);s.circle_y=std::clamp(s.circle_y,-1.f,1.f);
}
}
