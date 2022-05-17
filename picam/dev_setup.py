#!/usr/bin/env python3

import os
import venv
from pathlib import Path
from subprocess import check_call

source_dir = Path(__file__).resolve().parent

print()
print(f"=== System packages (sudo apt install ...) ===")
check_call([
    "sudo", "apt", "install",
    "ffmpeg", "libatlas-base-dev", "libcap-dev", "libcamera-dev",
    "python3-prctl", "python3-pyqt5",
])

print()
print(f"=== Python packages (pip install ...) ===")
venv_dir = source_dir / "python_venv"
venv_bin = venv_dir / "bin"
if not venv_dir.is_dir():
    print(f"Creating {venv_dir}...")
    os.environ.pop("PYTHONPATH", None)
    venv.create(venv_dir, symlinks=True, with_pip=True)
    check_call(["direnv", "allow", source_dir])

os.environ["PYTHONPATH"] = "/usr/lib/python3/dist-packages"
check_call([
    venv_bin / "pip", "install", "picamera2",
    "--no-binary", "simplejpeg"
])
