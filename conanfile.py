from conan import ConanFile


class EntelechyConan(ConanFile):
    name = "entelechy"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("glfw/3.4")
        # Future dependencies:
        # self.requires("spdlog/1.14.1")
        # self.requires("glm/1.0.1")
        # self.requires("gtest/1.14.0")
