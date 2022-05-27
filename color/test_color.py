#!/usr/bin/env python3
#
# Exercise access to TCS34725 color sensor via Raspberry Pi I2C bus
# https://cdn-shop.adafruit.com/datasheets/TCS34725.pdf
# https://github.com/kplindegaard/smbus

import argparse
import struct
import time

import smbus2 as smbus

parser = argparse.ArgumentParser(description="Exercise TCS34725 over I2C")
parser.add_argument("--bus", type=int, default=1)
parser.add_argument("--msec", type=float, default=2.4)
parser.add_argument("--gain", type=int, default=1)
args = parser.parse_args()

bus = smbus.SMBus(args.bus)

gain = args.gain
again = 0 if gain <= 2 else 1 if gain <= 8 else 2 if gain <= 30 else 3
atime = max(0, min(255, 256 - round(args.msec / 2.4)))

for msg in [
    smbus.i2c_msg.write(0x29, [0x80 | 0x00 | 0x00, 0x00]),   # Disable
    smbus.i2c_msg.write(0x29, [0x80 | 0x00 | 0x01, atime]),
    smbus.i2c_msg.write(0x29, [0x80 | 0x00 | 0x0f, again]),
    smbus.i2c_msg.write(0x29, [0x80 | 0x00 | 0x00, 0x03]),   # Enable
]:
    bus.i2c_rdwr(msg)

time.sleep(0.0024 * (256 - atime + 1))
while True:
    get_status = smbus.i2c_msg.read(0x29, 1)
    bus.i2c_rdwr(smbus.i2c_msg.write(0x29, [0x80 | 0x00 | 0x13]), get_status)
    status, = struct.unpack("B", get_status.buf[0])
    if status & 0x01: break  # AVALID indicates cycle complete
    time.sleep(0.005)  # If not, delay a bit and try again

get_all = smbus.i2c_msg.read(0x29, 0x1c)
bus.i2c_rdwr(smbus.i2c_msg.write(0x29, [0x80 | 0x20 | 0x00]), get_all)

(
    enable, atime, wtime, ailt, aiht, pers, config, control,
    id, status, cdata, rdata, gdata, bdata,
) = struct.unpack("<BBxBHHxxxxBBxBxxBBHHHH", get_all.buf[:get_all.len])

print(
    f"ENABLE = 0x{enable:02x} ("
    f"{'' if enable & 0x10 else '!'}AIEN "
    f"{'' if enable & 0x08 else '!'}WEN "
    f"{'' if enable & 0x02 else '!'}AEN "
    f"{'' if enable & 0x01 else '!'}PON)"
)

print(f"ATIME = 0x{atime:02x} ({(256 - atime) * 2.4:.1f}ms)")

wait_step = 0.029 if (config & 0x2) else 0.0024
print(f"WTIME = 0x{wtime:02x} ({(256 - wtime) * wait_step:.1f}ms)")
print(f"AILT = 0x{ailt:04x}")
print(f"AIHT = 0x{aiht:04x}")

pers_cycles = {0: 0, 1: 1, 2: 2, 3: 3}.get(pers, (pers - 3) * 5)
print(f"PERS = 0x{pers:02x} ({pers_cycles} cycles)")
print(f"CONFIG = 0x{config:02x} ({'' if config & 0x02 else '!'}WLONG)")

gain = {0: "1x", 1: "4x", 2: "16x", 3: "60x"}.get(control, "Unknown")
print(f"CONTROL = 0x{control:02x} ({gain} gain)")

device = {0x44: "TCS34721/5", 0x4d: "TCS34723/7"}.get(id, "Unknown")
print(f"ID = 0x{id:02x} ({device})")

print(
    f"STATUS = 0x{status:02x} ("
    f"{'' if status & 0x10 else '!'}AINT "
    f"{'' if status & 0x01 else '!'}AVALID)"
)

for chan, data in {"C": cdata, "R": rdata, "G": gdata, "B": bdata}.items():
    percent = data / ((256 - atime) * 1024) * 100
    sat = ' ASAT' if percent == 100.0 else ' DSAT' if data == 0xffff else ''
    print(f"{chan}DATA = 0x{data:04x} ({percent:.1f}%{sat})")
