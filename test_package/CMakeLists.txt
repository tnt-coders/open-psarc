cmake_minimum_required(VERSION 3.16)
project(PackageTest CXX)

find_package(OpenPSARC REQUIRED CONFIG)

add_executable(test_package test_package.cpp)
target_link_libraries(test_package OpenPSARC::OpenPSARC)
