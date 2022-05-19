# See https://docs.conan.io/en/latest/reference/conanfile.html

import conan.tools.system.package_manager
import conans
import glob
import os.path
import shutil

class PicamConan(conans.ConanFile):
    name, version = "picam", "0.0"
    settings = "os", "compiler", "build_type", "arch"  # boilerplate
    generators = "pkg_config"  # Used by the Meson build helper (below)
    options = {"shared": [True, False]}
    default_options = {"shared": False}  # Used by Meson build helper

    requires = [
        "cli11/2.1.1", "concurrentqueue/1.0.3", "fmt/8.0.1", "libpng/1.6.37",
    ]

    def build(self):
        meson = conans.Meson(self)  # Uses the "pkg_config" generator (above)
        meson_private_dir = os.path.join(self.build_folder, "meson-private")
        shutil.rmtree(meson_private_dir, ignore_errors=True)  # Force reconfig
        meson.configure()
        meson.build()
