from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
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
        "build_cli": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_cli": True,
    }
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cli/*", "cmake/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("cmake-package-builder/1.0.0")
        self.requires("zlib/1.3.1")
        self.requires("xz_utils/5.8.1")
        self.requires("openssl/3.6.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["OPEN_PSARC_BUILD_CLI"] = self.options.build_cli
        tc.variables["CONAN_EXPORT"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):fffffffffffffff
        cmake = CMake(self)
        cmake.install()
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"), keep_path=True)

    def package_info(self):
        # Let CMake's own package config files define everything
        self.cpp_info.set_property("cmake_find_mode", "config")
        self.cpp_info.set_property("cmake_file_name", "OpenPSARC")
