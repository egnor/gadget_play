#!/usr/bin/env python3

import subprocess

from bluepy.btle import helperExe

subprocess.check_call(["sudo", "setcap", "cap_net_admin=ep", helperExe])
