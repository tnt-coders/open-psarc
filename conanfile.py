from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
import os

class OpenPsarcConan(ConanFile):
    name = "open-psarc"
    license = "MIT"
    author = "TNT Coders <tnt-coders@googlegroups.com>"
    url = "https://github.com/tnt-coders/open-psarc"
    description = "PSARC archive reader"
    topics = ("psarc", "archive", "compression")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    exports_sources = "CMakeLists.txt", "project-config/*", "src/*", "include/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("cmake-package-builder/1.0.0") #recipe: https://github.com/tnt-coders/cmake-package-builder.git
        self.requires("wwise-audio-tools/master") #recipe: https://github.com/tnt-coders/wwise-audio-tools.git
        self.requires("zlib/1.3.1")
        self.requires("xz_utils/5.8.1")
        self.requires("openssl/3.6.1")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_CLI"] = False
        tc.variables["PROJECT_CONFIG_ENABLE_DOCS"] = False
        tc.variables["PROJECT_CONFIG_ENABLE_CLANG_TIDY"] = False
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenPSARC")
        self.cpp_info.set_property("cmake_target_name", "OpenPSARC::OpenPSARC")
        self.cpp_info.libs = ["OpenPSARC"]
