#pragma once
#include <mk7/host/input.hpp>
#include <mk7/recomp/runtime.hpp>
#include <memory>
#include <string>
namespace mk7::host {
class Application {
public:
 Application(); ~Application();
 Application(const Application&)=delete; Application& operator=(const Application&)=delete;
 void initialize(const std::string& title);
 [[nodiscard]] bool poll();
 void render(const RuntimeSnapshot& snapshot);
private:
 struct Impl; std::unique_ptr<Impl> impl_;
};
}
