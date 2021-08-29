#!/usr/bin/env python3

import subprocess

from bluepy.btle import helperExe  # type: ignore

subprocess.run(["sudo", "setcap", "cap_net_admin=ep", helperExe], check=True)
