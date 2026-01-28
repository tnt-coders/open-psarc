cmake_minimum_required(VERSION 3.16)
project(PackageTest CXX)

find_package(open-psarc REQUIRED CONFIG)

add_executable(test_package test_package.cpp)
target_link_libraries(test_package tnt::open-psarc)
target_compile_features(test_package PRIVATE cxx_std_23)

enable_testing()
add_test(NAME test_package COMMAND test_package)
