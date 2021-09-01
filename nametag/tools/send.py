#!/usr/bin/env python3

import argparse
import logging
import sys
import time
from pathlib import Path
from typing import List, Tuple

import PIL.Image  # type: ignore

sys.path.append(str(Path(__file__).parent.parent))
import nametag.bluetooth
import nametag.logging
import nametag.protocol

logging.getLogger().setLevel(logging.DEBUG)

parser = argparse.ArgumentParser()
parser.add_argument("--interface", type=int, default=0, help="HCI interface")
parser.add_argument("--msec", type=int, default=200, help="Frame time")
device_args = parser.add_mutually_exclusive_group()
device_args.add_argument("--address", help="MAC to address")
device_args.add_argument("--code", help="Device code to find")
show_args = parser.add_mutually_exclusive_group(required=True)
show_args.add_argument("--packets", nargs="+", help="raw packets hex file")
show_args.add_argument("--frames", nargs="+", help="animation images")
show_args.add_argument("--glyphs", nargs="+", help="glyph images")
parser.add_argument("--mode", type=int, help="mode to set")
parser.add_argument("--speed", type=int, help="speed to set")
parser.add_argument("--brightness", type=int, help="brightness to set")
args = parser.parse_args()

send_expect: List[Tuple[bytes, bytes]] = []

if args.packets:
    print(f"=== Loading packets ===")
    for packets_path in args.packets:
        print(packets_path)
        with open(packets_path) as hex_file:
            for line in hex_file:
                line = line.strip().replace(":", " ")
                pair = (bytes.fromhex(line), b"") if line else (b"", b"*")
                send_expect.append(pair)

if args.frames:
    print(f"=== Loading frames ===")
    frames: List[PIL.Image.Image] = []
    for frame_path in args.frames:
        print(frame_path)
        frame = PIL.Image.open(frame_path).convert(mode="1")
        frames.append(frame.resize((48, 12)))

    send_expect.extend(nametag.protocol.show_frames(frames, msec=args.msec))
    print()

if args.glyphs:
    print(f"=== Loading glyphs ===")
    glyphs: List[PIL.Image.Image] = []
    for glyph_path in args.glyphs:
        print(glyph_path)
        glyph = PIL.Image.open(glyph_path).convert(mode="1")
        new_width = glyph.size[0] * 12 // glyph.size[1]
        glyphs.append(glyph.resize((new_width, 12)))

    send_expect.extend(nametag.protocol.show_glyphs(glyphs))
    print()

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
