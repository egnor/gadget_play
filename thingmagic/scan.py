#!/usr/bin/env python3

import argparse
import time

import mercury

parser = argparse.ArgumentParser("test for ThingMagic UHF RFID reader")
parser.add_argument("--port", default="/dev/ttyACM2")
parser.add_argument("--antenna", default=2)
parser.add_argument("--power", default=0)
args = parser.parse_args()

reader = mercury.Reader(f"tmr://{args.port}")
print(f"Connected: {reader.get_model()} / {reader.get_serial()}")
print(f"Version: {reader.get_software_version()}")

reader.set_region("NA")
reader.set_read_plan([args.antenna], "GEN2", read_power=args.power)

tags = {}
while True:
    tags.update((r.epc, r) for r in reader.read(timeout=500))
    now = time.time()

    out = "\x1bc"
    for epc, data in sorted(tags.items()):
        out += (
            f"{now - data.timestamp:5.1f}s "
            f"{data.epc.decode()} "
            f"{data.rssi:4d}dB "
            f"{data.frequency // 1000}MHz \n"
        )

    print(out, end="", flush=True)
