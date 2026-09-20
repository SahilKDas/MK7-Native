#include <mk7/host/input.hpp>
#include <cassert>
using namespace mk7::host;
int main(){InputState s;update_key(s,Key::a,true);assert(s.buttons&static_cast<unsigned>(Button::a));update_key(s,Key::a,false);assert(!(s.buttons&static_cast<unsigned>(Button::a)));update_key(s,Key::circle_left,true);assert(s.circle_x==-1);update_key(s,Key::circle_right,true);assert(s.circle_x==1);update_key(s,Key::circle_right,false);assert(s.circle_x==0);update_key(s,Key::up,true);assert(s.buttons&static_cast<unsigned>(Button::dpad_up));}
