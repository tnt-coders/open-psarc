cmake_minimum_required(VERSION 3.16)

project(open-psarc
    VERSION 1.0.0
    DESCRIPTION "PSARC archive reader library"
    LANGUAGES CXX
)

option(OPEN_PSARC_BUILD_CLI "Build command-line interface" ON)

include(GNUInstallDirs)

# Find dependencies
find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(OpenSSL REQUIRED)

# Library
add_library(open-psarc src/psarc_file.cpp)
add_library(tnt::open-psarc ALIAS open-psarc)

target_compile_features(open-psarc PUBLIC cxx_std_23)

target_include_directories(open-psarc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(open-psarc
    PRIVATE
        ZLIB::ZLIB
        LibLZMA::LibLZMA
        OpenSSL::SSL
        OpenSSL::Crypto
)

set_target_properties(open-psarc PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# CLI
if(OPEN_PSARC_BUILD_CLI)
    add_executable(open-psarc-cli cli/main.cpp)

    target_link_libraries(open-psarc-cli PRIVATE open-psarc)

    set_target_properties(open-psarc-cli PROPERTIES
        OUTPUT_NAME open-psarc
    )

    install(TARGETS open-psarc-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install
install(TARGETS open-psarc
    EXPORT open-psarc-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT open-psarc-targets
    FILE open-psarc-targets.cmake
    NAMESPACE tnt::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/open-psarc
)

# Package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/open-psarc-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/open-psarc-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/open-psarc
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/open-psarc-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/open-psarc-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/open-psarc-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/open-psarc
)
