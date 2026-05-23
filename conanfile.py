from conan import ConanFile
from conan.tools.cmake import cmake_layout

class ComfortSelectDashboardRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        if self.settings.os != "Windows":
            self.requires("libusb/1.0.26")
        self.requires("sqlite3/3.45.0")
        self.requires("sqlite_orm/1.8.2")
        self.requires("spdlog/1.12.0")
        self.requires("nlohmann_json/3.11.2")
        self.requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)
