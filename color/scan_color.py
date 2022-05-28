#!/usr/bin/env python3
#
# Scan TCS34725 color sensor via Raspberry Pi I2C bus

import argparse
import struct
import time
from yachalk import chalk

import smbus2 as smbus

parser = argparse.ArgumentParser(description="Scan TCS34725 over I2C")
parser.add_argument("--bus", type=int, default=1)
parser.add_argument("--msec", type=float, default=2.4)
parser.add_argument("--gain", type=int, default=1)
parser.add_argument("--hcv", action="store_true")
args = parser.parse_args()

bus = smbus.SMBus(args.bus)
gain = args.gain
again = 0 if gain <= 2 else 1 if gain <= 8 else 2 if gain <= 30 else 3
atime = max(0, min(255, 256 - round(args.msec / 2.4)))
max_count = 1024 * (256 - atime)

for msg in [
    smbus.i2c_msg.write(0x29, [0x80, 0x00]),   # Disable
    smbus.i2c_msg.write(0x29, [0x81, atime]),
    smbus.i2c_msg.write(0x29, [0x8f, again]),
    smbus.i2c_msg.write(0x29, [0x80, 0x03]),   # Enable
]:
    bus.i2c_rdwr(msg)

start_time = time.time()
while True:  # Scan loop
    bus.i2c_rdwr(smbus.i2c_msg.write(0x29, [0x93]))
    get_status = smbus.i2c_msg.read(0x29, 1)
    while True:  # Poll for ready
        bus.i2c_rdwr(get_status)
        status, = struct.unpack("B", get_status.buf[0])
        if status & 0x10: break
        time.sleep(0.001)  # Keep polling

    out = f"{time.time() - start_time:7.3f}s"

    get_data = smbus.i2c_msg.read(0x29, 8)
    bus.i2c_rdwr(smbus.i2c_msg.write(0x29, [0x94]), get_data)
    bus.i2c_rdwr(smbus.i2c_msg.write(0x29, [0xE6]))  # Clear INT
    data = struct.unpack("<HHHH", get_data.buf[:get_data.len])
    frac = [c / max_count for c in data]
    sat = any(f >= 1.0 for f in frac[1:]) or any(d >= 0xffff for d in data[1:])

    if not args.hcv:
        for chan, v, c in zip("crgb", frac, ["FFF", "F00", "0F0", "00F"]):
            out += chalk.bold.hex(c)(f"{v * 100:3.0f}{chan}")
        out += " ["
        for p in range(1, 100, 2):
            rgb, ch = [0, 0, 0], "·"
            if p - 1 <= frac[0] * 100 < p + 1: rgb, ch = [128, 128, 128], "◆"
            if p - 1 <= frac[1] * 100 < p + 1: rgb[0] = 255
            if p - 1 <= frac[2] * 100 < p + 1: rgb[1] = 255
            if p - 1 <= frac[3] * 100 < p + 1: rgb[2] = 255
            out += ch if rgb == [0, 0, 0] else chalk.bg_rgb(*rgb).black(ch)
        out += "]"


    if args.hcv:
        r, g, b = frac[1:]
        m, M = min(r, g, b), max(r, g, b)
        C, V = M - m, M
        H = 0 if (m == M) else (
            240 + 60 * (r - g) / (M - m) if (b == M) else
            120 + 60 * (b - r) / (M - m) if (g == M) else
            (360 + 60 * (g - b) / (M - m)) % 360
        )

        blocks = (" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█")
        out += f" {H:3.0f}°{C * 100:3.0f}C{V * 100:3.0f}V ["
        for h in range(5, 360, 10):
            hr, hg, hb = (
                (255, h * 255 / 60, 0) if h < 60 else
                ((120 - h) * 255 / 60, 255, 0) if h < 120 else
                (0, 255, (h - 120) * 255 / 60) if h < 180 else
                (0, (240 - h) * 255 / 60, 255) if h < 240 else
                ((h - 240) * 255 / 60, 0, 255) if h < 300 else
                (255, 0, (360 - h) * 255 / 60)
            )
            if h - 5 <= H < h + 5:
                block = blocks[round(C * (len(blocks) - 1))]
                out += chalk.rgb(hr, hg, hb)(block)
            else:
                out += chalk.rgb(hr, hg, hb)("·")

        out += "] ["
        for v in range(5, 100, 10):
            if v - 5 <= V * 100 < v + 5:
                out += blocks[round(V * (len(blocks) - 1))]
            else:
                out += "·"
        out += "]"

    if sat: out += " SAT"
    print(out)
