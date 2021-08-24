#!/usr/bin/env python3

import os
from pathlib import Path
from subprocess import check_call

os.chdir(str(Path(__file__).parent.parent))

check_call(["black", "-l", "80", "."])
check_call(["isort", "."])
