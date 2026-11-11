import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class TestPackageConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps"

    def requirements(self):
        self.requires(self.tested_reference_str)
        # CMakeDeps only generates config files for direct requirements, so
        # declare here everything that test_package/CMakeLists.txt find_packages.
        self.requires("sqlite3/3.45.0")
        self.requires("sqlite_orm/1.8.2")
        self.requires("spdlog/1.12.0")
        self.requires("nlohmann_json/3.11.2")
        # On Windows libusb comes from MSYS2's pkg-config metadata.
        if self.settings.os != "Windows":
            self.requires("libusb/1.0.26")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["CMAKE_CXX_STANDARD"] = 20
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            cmd = os.path.join(self.cpp.build.bindirs[0], "test_package")
            self.run(cmd, env="conanrun")
