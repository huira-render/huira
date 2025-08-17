from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeDeps, CMakeToolchain

class HuiraConan(ConanFile):
    name = "huira"
    description = "A ray tracing library for space engineering and planetary science."
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    
    # Package configuration
    options = {
        "tests": [True, False],
        "shared": [True, False],
        "fPIC": [True, False]
    }
    
    default_options = {
        "tests": False,
        "shared": False,
        "fPIC": True
    }
    
    def layout(self):
        cmake_layout(self)
    
    def configure(self):
        # Remove fPIC option for Windows
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

        # Ensure C++20 is used
        if self.settings.compiler.cppstd:
            self.settings.compiler.cppstd = "20"
    
    def requirements(self):
        # Core dependencies with version constraints
        self.requires("assimp/5.4.3")
        self.requires("cfitsio/4.4.0")
        self.requires("cspice/0067")
        self.requires("embree3/3.13.5")
        self.requires("fftw/3.3.10")
        self.requires("gdal/3.10.3")
        self.requires("glm/1.0.1")
        self.requires("libtiff/4.6.0")
        self.requires("onetbb/[>=2021.0]")  # Needs flexible range for Embree compatability
    
         # Test dependencies
        if self.options.tests:
            self.requires("gtest/1.16.0")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.15]")
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()
    
    def build(self):
        pass
    
    def package(self):
        pass
    
    def package_info(self):
        pass