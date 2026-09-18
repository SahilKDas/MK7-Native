#include <mk7/formats/format.hpp>

#include <array>

namespace mk7::formats {
namespace {

using Magic = std::array<char, 4>;

[[nodiscard]] auto has_magic(
    std::span<const std::byte> data,
    std::size_t offset,
    Magic magic) noexcept -> bool {
    if (data.size() < offset + magic.size()) {
        return false;
    }
    for (std::size_t index = 0; index < magic.size(); ++index) {
        if (data[offset + index] != static_cast<std::byte>(magic[index])) {
            return false;
        }
    }
    return true;
}

} // namespace

auto detect(std::span<const std::byte> data) noexcept -> Format {
    if (has_magic(data, 0, {'S', 'A', 'R', 'C'})) return Format::sarc;
    if (has_magic(data, 0, {'Y', 'a', 'z', '0'})) return Format::yaz0;
    if (has_magic(data, 0, {'C', 'G', 'F', 'X'})) return Format::bcmdl;
    if (has_magic(data, 0, {'C', 'T', 'E', 'X'})) return Format::bctex;
    if (has_magic(data, 0, {'C', 'L', 'Y', 'T'})) return Format::bclyt;
    if (has_magic(data, 0, {'C', 'W', 'A', 'V'})) return Format::bcwav;
    if (has_magic(data, 0, {'C', 'S', 'T', 'M'})) return Format::bcstm;
    if (has_magic(data, 0, {'C', 'S', 'A', 'R'})) return Format::bcsar;
    if (has_magic(data, 0, {'C', 'G', 'R', 'P'})) return Format::bcgrp;
    if (has_magic(data, 0, {'M', 'A', 'T', 'A'})) return Format::bcmata;
    if (has_magic(data, 0, {'M', 'C', 'L', 'A'})) return Format::bcmcla;

    // BCLIM stores its identification block at the end of the file.
    if (data.size() >= 0x28 && has_magic(data, data.size() - 0x28, {'C', 'L', 'I', 'M'})) {
        return Format::bclim;
    }
    return Format::unknown;
}

auto name(Format format) noexcept -> std::string_view {
    switch (format) {
    case Format::sarc: return "SARC";
    case Format::yaz0: return "Yaz0";
    case Format::bcmdl: return "BCMDL/CGFX";
    case Format::bctex: return "BCTEX";
    case Format::bclim: return "BCLIM";
    case Format::bclyt: return "BCLYT";
    case Format::bcwav: return "BCWAV";
    case Format::bcstm: return "BCSTM";
    case Format::bcsar: return "BCSAR";
    case Format::bcgrp: return "BCGRP";
    case Format::bcmata: return "BCMATA";
    case Format::bcmcla: return "BCMCLA";
    case Format::unknown: return "unknown";
    }
    return "unknown";
}

auto description(Format format) noexcept -> std::string_view {
    switch (format) {
    case Format::sarc: return "Nintendo SARC archive";
    case Format::yaz0: return "Nintendo Yaz0-compressed data";
    case Format::bcmdl: return "NintendoWare CTR graphics/model container";
    case Format::bctex: return "NintendoWare CTR texture";
    case Format::bclim: return "NintendoWare CTR image";
    case Format::bclyt: return "NintendoWare CTR UI layout";
    case Format::bcwav: return "NintendoWare CTR wave audio";
    case Format::bcstm: return "NintendoWare CTR streamed audio";
    case Format::bcsar: return "NintendoWare CTR sound archive";
    case Format::bcgrp: return "NintendoWare CTR sound group";
    case Format::bcmata: return "Mario Kart 7 material animation";
    case Format::bcmcla: return "Mario Kart 7 material color animation";
    case Format::unknown: return "unrecognized data";
    }
    return "unrecognized data";
}

} // namespace mk7::formats

