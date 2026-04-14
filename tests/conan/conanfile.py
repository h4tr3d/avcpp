from conan import ConanFile

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
    generators = 'PkgConfigDeps'

    def requirements(self):
      self.requires(f"ffmpeg/{self.options.ffmpeg}", transitive_headers=True)
      if self.settings.os == 'Windows':
         self.tool_requires("pkgconf/2.5.1")

    def configure(self):
      self.options["ffmpeg"].fPIC = True
      self.options["ffmpeg"].shared = True
      self.options["ffmpeg"].postproc = True
      