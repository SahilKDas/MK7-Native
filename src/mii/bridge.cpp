#include <mk7/mii/bridge.hpp>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
namespace mk7::mii {
namespace {
std::wstring quote(const std::filesystem::path& path){auto v=path.wstring();if(v.contains(L'"'))throw std::runtime_error("quotes are not supported in bridge paths");return L"\""+v+L"\"";}
std::string run(const std::filesystem::path& executable,const std::vector<std::filesystem::path>& arguments,bool capture){
#ifdef _WIN32
 std::wstring command=quote(executable);for(const auto& a:arguments)command+=L" "+quote(a);
 std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(0);
 SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};HANDLE read_pipe{},write_pipe{};
 if(capture&&!CreatePipe(&read_pipe,&write_pipe,&security,0))throw std::runtime_error("cannot create Mii Bridge output pipe");
 if(read_pipe)SetHandleInformation(read_pipe,HANDLE_FLAG_INHERIT,0);
 STARTUPINFOW startup{};startup.cb=sizeof(startup);if(capture){startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=write_pipe;startup.hStdError=GetStdHandle(STD_ERROR_HANDLE);startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);}
 PROCESS_INFORMATION process{};
 if(!CreateProcessW(executable.wstring().c_str(),buffer.data(),nullptr,nullptr,capture?TRUE:FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)){if(read_pipe)CloseHandle(read_pipe);if(write_pipe)CloseHandle(write_pipe);throw std::runtime_error("cannot launch MK7 Mii Bridge");}
 if(write_pipe)CloseHandle(write_pipe);std::string output;
 if(read_pipe){std::array<char,512> chunk{};DWORD count{};while(ReadFile(read_pipe,chunk.data(),DWORD(chunk.size()),&count,nullptr)&&count)output.append(chunk.data(),count);CloseHandle(read_pipe);}
 WaitForSingleObject(process.hProcess,INFINITE);DWORD code{};GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);
 if(code)throw std::runtime_error("MK7 Mii Bridge command failed with exit code "+std::to_string(code));return output;
#else
 (void)executable;(void)arguments;(void)capture;throw std::runtime_error("MK7 Mii Bridge adapter currently requires Windows");
#endif
}
}
BridgeAdapter::BridgeAdapter(std::filesystem::path executable,std::filesystem::path profile):executable_(std::move(executable)),profile_(std::move(profile)){}
bool BridgeAdapter::available()const noexcept{std::error_code ec;return std::filesystem::is_regular_file(executable_,ec);}
BridgeArtifacts BridgeAdapter::prepare(const std::filesystem::path& session)const{
 if(!available())throw std::runtime_error("MK7 Mii Bridge executable does not exist");
 std::filesystem::create_directories(session);if(profile_.has_parent_path())std::filesystem::create_directories(profile_.parent_path());
 auto capabilities=run(executable_,{"capabilities"},true);if(capabilities.find("\"protocol\":1")==std::string::npos)throw std::runtime_error("incompatible MK7 Mii Bridge protocol");
 if(!std::filesystem::is_regular_file(profile_))run(executable_,{"create-default",profile_},false);
 const auto preview=session/"player.ppm",romfs=session/"mii-bridge.romfs";
 run(executable_,{"render",profile_,preview},false);run(executable_,{"build-romfs",profile_,romfs},false);
 if(!std::filesystem::is_regular_file(preview)||!std::filesystem::is_regular_file(romfs))throw std::runtime_error("MK7 Mii Bridge did not produce required artifacts");
 return {profile_,preview,romfs,std::move(capabilities)};
}
}
