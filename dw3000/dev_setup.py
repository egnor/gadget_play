#!/usr/bin/env python3

import os
import signal
from pathlib import Path
from subprocess import check_call, check_output

signal.signal(signal.SIGINT, signal.SIG_DFL)
source_dir = Path(__file__).resolve().parent

print("=== System packages (sudo apt install ...) ===")
apt_packages = [
    "cargo", "direnv", "libffi-dev", "libssl-dev",
    "python3", "python3-dev", "python3-pip",
]
installed = check_output(["dpkg-query", "--show", "--showformat=${Package}\\n"])
installed = installed.decode().split()
if not all(p in installed for p in apt_packages):
    check_call(["sudo", "apt", "install"] + apt_packages)

import venv  # In case it just got installed above.
print()
print(f"=== Python packages (pip install ...) ===")
venv_dir = source_dir / "build" / "python_venv"
venv_bin = venv_dir / "bin"
if not venv_dir.is_dir():
    print(f"Creating {venv_dir}...")
    venv_dir.parent.mkdir(exist_ok=True, parents=True)
    venv.create(venv_dir, symlinks=True, with_pip=True)

pips = ["platformio", "wheel"]
check_call(["direnv", "allow", source_dir])
check_call(["direnv", "exec", source_dir, "pip", "install", *pips])

print()
print("::: Setup complete :::")
