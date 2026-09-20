#pragma once
#include <array>
namespace mk7::host {
struct ScreenRect { int x{},y{},width{},height{}; };
[[nodiscard]] std::array<ScreenRect,2> compute_screen_layout(int width,int height) noexcept;
}
