from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain
import os

required_conan_version = ">=2.0"

class Avcpp(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "ffmpeg": ["8.0.1", "7.1.3", "6.1.1"],
        "build": [ "CMake", "meson" ]
    }
    default_options = {
        "ffmpeg": "8.0.1",
        "build": "CMake"
    }

    def requirements(self):
      self.requires(f"ffmpeg/{self.options.ffmpeg}", transitive_headers=True)

    def generate(self):
      tc = CMakeToolchain(self)
      tc.generate()

      deps = CMakeDeps(self)
      deps.set_property("ffmpeg", "cmake_file_name", "FFmpeg")
      deps.set_property("ffmpeg", "cmake_target_name", "FFmpeg::FFmpeg")
      deps.generate()
