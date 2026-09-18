#include <mk7/formats/format.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

namespace {

auto inspect(const std::filesystem::path& path) -> bool {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        std::cerr << path.string() << ": unable to open file\n";
        return false;
    }
    const std::vector<char> chars{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
    const auto format = mk7::formats::detect(std::as_bytes(std::span{chars}));
    std::cout << path.string() << ": " << mk7::formats::name(format)
              << " (" << mk7::formats::description(format) << ")\n";
    return format != mk7::formats::Format::unknown;
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        std::cerr << "usage: mk7-inspect <file> [file ...]\n";
        return 2;
    }
    auto success = true;
    for (auto index = 1; index < argc; ++index) {
        success = inspect(argv[index]) && success;
    }
    return success ? 0 : 1;
}

