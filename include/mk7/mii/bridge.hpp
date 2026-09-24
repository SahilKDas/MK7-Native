#pragma once
#include <filesystem>
#include <string>
namespace mk7::mii {
struct BridgeArtifacts {
 std::filesystem::path profile;
 std::filesystem::path preview;
 std::filesystem::path romfs;
 std::string capabilities;
};
class BridgeAdapter {
public:
 BridgeAdapter(std::filesystem::path executable,std::filesystem::path profile);
 [[nodiscard]] bool available() const noexcept;
 BridgeArtifacts prepare(const std::filesystem::path& session_directory) const;
private:
 std::filesystem::path executable_,profile_;
};
}
