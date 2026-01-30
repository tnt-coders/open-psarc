cmake_minimum_required(VERSION 3.16)

project(OpenPSARC VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_CLI "Build command-line interface" ON)
option(CONAN_EXPORT "Export a Conan package" OFF)

# Find dependencies
find_package(PackageBuilder REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(OpenSSL REQUIRED)

include(PackageBuilder)
package_init()

# Library
package_add_library(OpenPSARC src/psarc_file.cpp)

target_compile_features(OpenPSARC PUBLIC cxx_std_23)

target_include_directories(OpenPSARC
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(OpenPSARC
    PRIVATE
        ZLIB::ZLIB
        LibLZMA::LibLZMA
        OpenSSL::SSL
        OpenSSL::Crypto
)

# CLI
if(BUILD_CLI)
    package_add_executable(OpenPSARC_CLI cli/main.cpp)

    target_link_libraries(OpenPSARC_CLI PRIVATE OpenPSARC)

    set_target_properties(OpenPSARC_CLI PROPERTIES
        OUTPUT_NAME open-psarc
    )
endif()

package_install()

package_export(CONAN_PACKAGE open-psarc)
