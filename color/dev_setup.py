#!/usr/bin/env python3

import os
import signal
import venv
from pathlib import Path
from subprocess import check_call, check_output

signal.signal(signal.SIGINT, signal.SIG_DFL)
source_dir = Path(__file__).resolve().parent

print()
print(f"=== Python packages (pip install ...) ===")
venv_dir = source_dir / "python_venv"
venv_bin = venv_dir / "bin"
if not venv_dir.is_dir():
    print(f"Creating {venv_dir}...")
    venv.create(venv_dir, symlinks=True, with_pip=True)

pips = ["smbus2", "yachalk"]
check_call(["direnv", "allow", source_dir])
check_call(["direnv", "exec", source_dir, "pip", "install", *pips])

print()
print("::: Setup complete :::")
