from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
import os


class OpenPsarcConan(ConanFile):
    name = "open-psarc"
    version = "1.0.0"
    license = "MIT"
    author = "Your Name <your.email@example.com>"
    url = "https://github.com/yourusername/open-psarc"
    description = "PSARC archive reader library"
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
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"), keep_path=True)

    def package_info(self):
        self.cpp_info.libs = ["open-psarc"]
        self.cpp_info.set_property("cmake_file_name", "open-psarc")
        self.cpp_info.set_property("cmake_target_name", "tnt::open-psarc")

        # For legacy generators
        self.cpp_info.names["cmake_find_package"] = "open-psarc"
        self.cpp_info.names["cmake_find_package_multi"] = "open-psarc"
        self.cpp_info.filenames["cmake_find_package"] = "open-psarc"
        self.cpp_info.filenames["cmake_find_package_multi"] = "open-psarc"

        if self.options.build_cli:
            bindir = os.path.join(self.package_folder, "bin")
            self.buildenv_info.prepend_path("PATH", bindir)
