cmake_minimum_required(VERSION 3.24)

project(
    OpenPSARC
    VERSION 1.0.0
    LANGUAGES CXX)

# Specify options
option(BUILD_CLI "Build command-line interface" ON)

# Add project configuration
add_subdirectory(project-config)

# Find dependencies
find_package(PackageBuilder REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(OpenSSL REQUIRED)

include(PackageBuilder)

# Create the package
package_create()

# Library
package_add_library(OpenPSARC src/psarc_file.cpp)

target_compile_features(OpenPSARC PUBLIC cxx_std_23)

target_link_libraries(OpenPSARC PRIVATE ZLIB::ZLIB LibLZMA::LibLZMA OpenSSL::SSL OpenSSL::Crypto)

# CLI
if(BUILD_CLI)
    package_add_executable(OpenPSARC_CLI cli/main.cpp)

    target_link_libraries(OpenPSARC_CLI PRIVATE OpenPSARC)

    set_target_properties(OpenPSARC_CLI PROPERTIES OUTPUT_NAME open-psarc)
endif()

package_install()
