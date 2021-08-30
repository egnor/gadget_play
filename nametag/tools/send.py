#!/usr/bin/env python3

import argparse
import logging
import sys
import time
from pathlib import Path
from typing import List, Tuple

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
source_args = parser.add_argument_group()
source_args.add_argument("--packets", help="raw packets hex file")
source_args.add_argument("--glyphs", help="glyph text hex file")
source_args.add_argument("--mode", type=int, help="mode to set")
source_args.add_argument("--speed", type=int, help="speed to set")
source_args.add_argument("--brightness", type=int, help="brightness to set")
args = parser.parse_args()

send_expect: List[Tuple[bytes, bytes]] = []

if args.packets:
    print(f"=== Loading packets: {args.packets} ===")
    with open(args.packets) as hex_file:
        for line in hex_file:
            line = line.strip().replace(":", " ")
            pair = (bytes.fromhex(line), b"") if line else (b"", b"*")
            send_expect.append(pair)

if args.glyphs:
    print(f"=== Loading glyphs: {args.glyphs} ===")
    glyphs: List[bytes] = []
    with open(args.glyphs) as hex_file:
        for line in hex_file:
            line = line.strip().replace(":", " ")
            if line:
                glyphs.append(bytes.fromhex(line))

    send_expect.extend(nametag.protocol.show_glyphs(glyphs))

if args.mode is not None:
    print(f"=== Setting mode {args.mode} ===")
    send_expect.extend(nametag.protocol.set_mode(args.mode))

if args.speed is not None:
    print(f"=== Setting speed {args.speed} ===")
    send_expect.extend(nametag.protocol.set_speed(args.speed))

if args.brightness is not None:
    print(f"=== Setting brightness {args.brightness} ===")
    send_expect.extend(nametag.protocol.set_brightness(args.brightness))

print(f"{len(send_expect)} packets loaded")
print()

print("=== Connecting to nametag ===")
with nametag.bluetooth.retry_connection(
    address=args.address, code=args.code, interface=args.interface
) as tag:
    tag.send_and_expect(pairs=send_expect)
    print()
