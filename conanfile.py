from conan import ConanFile
from conan.tools.cmake import cmake_layout


class ComfortSelectDashboardRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        # libscale is a separate project; create it first with:
        #   conan create libscale --build=missing -s build_type=Release
        self.requires("libscale/1.0.0")
        self.requires("spdlog/1.12.0")
        self.requires("nlohmann_json/3.11.2")
        self.requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)
