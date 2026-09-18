#include <mk7/formats/format.hpp>

#include <array>
#include <cassert>
#include <cstddef>

namespace {

template <std::size_t Size>
auto bytes(const char (&text)[Size]) {
    std::array<std::byte, Size - 1> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::byte>(text[index]);
    }
    return result;
}

} // namespace

auto main() -> int {
    using enum mk7::formats::Format;
    assert(mk7::formats::detect(bytes("SARC")) == sarc);
    assert(mk7::formats::detect(bytes("Yaz0")) == yaz0);
    assert(mk7::formats::detect(bytes("CGFX")) == bcmdl);
    assert(mk7::formats::detect(bytes("CSTM")) == bcstm);
    assert(mk7::formats::detect(bytes("nope")) == unknown);
    assert(mk7::formats::detect({}) == unknown);

    std::array<std::byte, 0x28> image{};
    image[0] = std::byte{'C'};
    image[1] = std::byte{'L'};
    image[2] = std::byte{'I'};
    image[3] = std::byte{'M'};
    assert(mk7::formats::detect(image) == bclim);
    return 0;
}
