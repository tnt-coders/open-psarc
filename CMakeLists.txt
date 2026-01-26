cmake_minimum_required(VERSION 3.16)

project(open-psarc
    VERSION 1.0.0
    DESCRIPTION "PSARC archive reader library"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(OPEN_PSARC_BUILD_CLI "Build command-line interface" ON)
option(BUILD_SHARED_LIBS "Build shared library" OFF)

# Find dependencies
find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(OpenSSL REQUIRED)

# Library target
add_library(open-psarc
    src/psarc_file.cpp
)

target_include_directories(open-psarc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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
    PUBLIC_HEADER include/open-psarc/psarc_file.h
)

# CLI executable
if(OPEN_PSARC_BUILD_CLI)
    add_executable(open-psarc-cli
        cli/main.cpp
    )

    target_link_libraries(open-psarc-cli
        PRIVATE
            open-psarc
    )

    set_target_properties(open-psarc-cli PROPERTIES
        OUTPUT_NAME open-psarc
    )

    install(TARGETS open-psarc-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install library
include(GNUInstallDirs)

install(TARGETS open-psarc
    EXPORT open-psarc-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/open-psarc
)

# Export targets for find_package support
install(EXPORT open-psarc-targets
    FILE open-psarc-targets.cmake
    NAMESPACE open-psarc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/open-psarc
)

# Package config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/open-psarc-config.cmake.in
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
