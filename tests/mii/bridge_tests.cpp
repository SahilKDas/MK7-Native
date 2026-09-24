#include <mk7/mii/bridge.hpp>
#include <cassert>
#include <filesystem>
int main(){
 const auto root=std::filesystem::temp_directory_path()/"mk7-native-mii-adapter-test";
 std::error_code ec;std::filesystem::remove_all(root,ec);std::filesystem::create_directories(root);
 mk7::mii::BridgeAdapter bridge{MK7_TEST_MII_BRIDGE,root/"player.mii.json"};
 assert(bridge.available());const auto artifacts=bridge.prepare(root/"session");
 assert(artifacts.capabilities.find("\"protocol\":1")!=std::string::npos);
 assert(std::filesystem::file_size(artifacts.profile)>32);
 assert(std::filesystem::file_size(artifacts.preview)>1000);
 assert(std::filesystem::file_size(artifacts.romfs)>0x1100);
 std::filesystem::remove_all(root,ec);
}
