#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace mk7::formats {

enum class Format {
    unknown,
    sarc,
    yaz0,
    bcmdl,
    bctex,
    bclim,
    bclyt,
    bcwav,
    bcstm,
    bcsar,
    bcgrp,
    bcmata,
    bcmcla,
};

[[nodiscard]] auto detect(std::span<const std::byte> data) noexcept -> Format;
[[nodiscard]] auto name(Format format) noexcept -> std::string_view;
[[nodiscard]] auto description(Format format) noexcept -> std::string_view;

} // namespace mk7::formats

