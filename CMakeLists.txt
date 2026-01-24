cmake_minimum_required(VERSION 3.16)

project(open-psarc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find compression libraries via Conan
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(open-psarc
    main.cpp
    psarcfile.h
    psarcfile.cpp
)

# Link compression libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ZLIB::ZLIB
        OpenSSL::SSL
        OpenSSL::Crypto
)

include(GNUInstallDirs)
install(TARGETS open-psarc
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
