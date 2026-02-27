cmake_minimum_required(VERSION 3.24)

# Define the project
project(
    OpenPSARC
    VERSION 1.0.0
    DESCRIPTION "Open source library to extract PSARC files"
    LANGUAGES C CXX)

# CMAKE_CXX_STANDARD must be set explicitly for Conan to detect it
set(CMAKE_CXX_STANDARD 23)

# Specify options
option(BUILD_CLI "Build command-line interface" ON)

# Add project configuration (disabled for Conan builds where project-config is not exported)
option(INCLUDE_PROJECT_CONFIG "Include project-config for docs, linting, and CMake presets" ON)
if(INCLUDE_PROJECT_CONFIG)

    # Set project-config specific options
    option(PROJECT_CONFIG_ENABLE_DOCS "Enable docs target when Doxygen is available" ON)
    option(PROJECT_CONFIG_ENABLE_CLANG_TIDY
           "Enable clang-tidy target when run-clang-tidy is available" ON)

    add_subdirectory(project-config)
endif()

# Find dependencies
find_package(PackageBuilder REQUIRED)
find_package(WwiseAudioTools REQUIRED)

find_package(LibLZMA REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(pugixml REQUIRED)
find_package(ZLIB REQUIRED)

include(PackageBuilder)

# Create the package
package_create()

# Library
package_add_library(OpenPSARC src/psarc_file.cpp src/sng_parser.cpp src/sng_xml_writer.cpp)

target_compile_features(OpenPSARC PUBLIC cxx_std_23)

target_link_libraries(
    OpenPSARC
    PRIVATE ZLIB::ZLIB LibLZMA::LibLZMA OpenSSL::SSL OpenSSL::Crypto pugixml::pugixml
            nlohmann_json::nlohmann_json
    PUBLIC WwiseAudioTools::WwiseAudioTools)

# CLI
if(BUILD_CLI)
    package_add_executable(OpenPSARC_CLI cli/main.cpp)

    target_link_libraries(OpenPSARC_CLI PRIVATE OpenPSARC)

    set_target_properties(OpenPSARC_CLI PROPERTIES OUTPUT_NAME open-psarc)
endif()

package_install()

# Tests
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
