cmake_minimum_required(VERSION 3.25)
project(MK7Native VERSION 0.1.0 LANGUAGES CXX)

option(MK7_NATIVE_BUILD_TESTS "Build the MK7-Native test suite" ON)

add_library(mk7_formats src/formats/format.cpp)
add_library(MK7Native::formats ALIAS mk7_formats)
target_compile_features(mk7_formats PUBLIC cxx_std_23)
target_include_directories(mk7_formats PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

if(MSVC)
    target_compile_options(mk7_formats PUBLIC /std:c++latest PRIVATE /W4 /permissive-)
else()
    target_compile_options(mk7_formats PUBLIC -std=c++2c PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(mk7-inspect apps/mk7-inspect/main.cpp)
target_compile_features(mk7-inspect PRIVATE cxx_std_23)
target_link_libraries(mk7-inspect PRIVATE MK7Native::formats)

if(MK7_NATIVE_BUILD_TESTS)
    include(CTest)
    add_executable(mk7_formats_tests tests/formats/format_tests.cpp)
    target_compile_features(mk7_formats_tests PRIVATE cxx_std_23)
    target_link_libraries(mk7_formats_tests PRIVATE MK7Native::formats)
    add_test(NAME mk7_formats_tests COMMAND mk7_formats_tests)
endif()

