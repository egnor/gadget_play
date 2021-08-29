#!/usr/bin/env python3

import argparse
import logging
import sys
import time
from pathlib import Path
from typing import List

import bluepy.btle  # type: ignore

sys.path.append(str(Path(__file__).parent.parent))
import nametag.bluetooth
import nametag.logging
import nametag.protocol

logging.getLogger().setLevel(logging.DEBUG)

parser = argparse.ArgumentParser()
parser.add_argument("--interface", type=int, default=0, help="HCI interface")
device_args = parser.add_mutually_exclusive_group()
device_args.add_argument("--address", help="MAC to address")
device_args.add_argument("--code", help="Device code to find")
source_args = parser.add_mutually_exclusive_group(required=True)
source_args.add_argument("--packets", help="raw packets hex file")
source_args.add_argument("--glyphs", help="glyph text hex file")
args = parser.parse_args()

packets: List[bytes] = []

if args.packets:
    print(f"=== Loading packets: {args.packets} ===")
    with open(args.packets) as hex_file:
        packets = [b""]
        for line in hex_file:
            line = line.strip().replace(":", " ")
            if not line:
                packets.extend([b""] if packets[-1] else [])
            elif not line.startswith("#"):
                packets[-1] += bytes.fromhex(line)
                while len(packets[-1]) > 20:
                    packets[-1:] = [packets[-1][:20], packets[-1][20:]]

if args.glyphs:
    print(f"=== Loading glyphs: {args.glyphs} ===")
    glyphs: List[bytes] = []
    with open(args.glyphs) as hex_file:
        for line in hex_file:
            line = line.strip().replace(":", " ")
            if line:
                glyphs.append(bytes.fromhex(line))

    packets = nametag.protocol.packets_from_glyphs(glyphs)

print(f"{len(packets)} packets loaded")
print()

print("=== Connecting to nametag ===")
with nametag.bluetooth.retry_device_open(
    address=args.address, code=args.code, interface=args.interface
) as tag:
    tag.send_packets(packets)
    print()
