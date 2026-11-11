import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class LibscaleConan(ConanFile):
    name = "libscale"
    version = "1.0.0"
    description = ("Library for reading Comfort Select USB scales and computing body "
                   "composition metrics from raw bio-impedance data.")
    license = "MIT"
    url = "https://github.com/SpeedProg/libscale"
    homepage = "https://github.com/SpeedProg/libscale"
    topics = ("scale", "bioimpedance", "health", "libusb", "sqlite")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}
    exports_sources = ("CMakeLists.txt", "cmake/*", "include/*", "src/*", "tests/*",
                       "tools/*", "LICENSE", "README.md")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        # On Windows libusb comes from the system (MSYS2 ships pkg-config metadata
        # for it), matching how the dashboard is built there.
        if self.settings.os != "Windows":
            self.requires("libusb/1.0.26")
        self.requires("sqlite3/3.45.0")
        self.requires("sqlite_orm/1.8.2")
        self.requires("spdlog/1.12.0")
        self.requires("nlohmann_json/3.11.2")

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        toolchain = CMakeToolchain(self)
        toolchain.variables["CMAKE_CXX_STANDARD"] = 20
        toolchain.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe("fPIC",
                                                                                       True)
        toolchain.variables["LIBSCALE_BUILD_TESTS"] = not self.conf.get("tools.build:skip_test",
                                                                        default=False)
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "libscale")
        self.cpp_info.set_property("cmake_target_name", "libscale::libscale")
        self.cpp_info.libs = ["libscale"]
