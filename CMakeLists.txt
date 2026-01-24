cmake_minimum_required(VERSION 3.16)

project(test_psarc LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# Find compression libraries via Conan
find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(test_psarc
  main.cpp
  psarcfile.h psarcfile.cpp
  conanfile.txt
)
# Link Qt and compression libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        ZLIB::ZLIB
        LibLZMA::LibLZMA
        OpenSSL::SSL
        OpenSSL::Crypto
)

include(GNUInstallDirs)
install(TARGETS test_psarc
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
