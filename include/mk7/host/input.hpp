#pragma once
#include <cstdint>

namespace mk7::host {
enum class Button : std::uint32_t {
    a=1u<<0,b=1u<<1,select=1u<<2,start=1u<<3,
    dpad_right=1u<<4,dpad_left=1u<<5,dpad_up=1u<<6,dpad_down=1u<<7,
    r=1u<<8,l=1u<<9,x=1u<<10,y=1u<<11
};
enum class Key { unknown,a,b,x,y,l,r,start,select,up,down,left,right,circle_up,circle_down,circle_left,circle_right };
struct InputState { std::uint32_t buttons{}; float circle_x{},circle_y{}; };
void update_key(InputState& state, Key key, bool pressed) noexcept;
}
