#include <mk7/recomp/runtime.hpp>

#include <stdexcept>

extern "C" void mk7_recomp_block_100000() {
    throw std::runtime_error{
        "this build has no generated entry block; configure MK7_NATIVE_GENERATED_ENTRY"};
}

