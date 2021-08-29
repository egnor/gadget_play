#!/usr/bin/env python3

import argparse
import sys

import bluepy.btle  # type: ignore

any_base_int = lambda x: int(x, 0)

parser = argparse.ArgumentParser()
parser.add_argument("--interface", type=int, default=0, help="HCI interface")
parser.add_argument("--address", help="MAC to address")
parser.add_argument("--service", type=any_base_int, default=0xFFF0)
parser.add_argument("--characteristic", type=any_base_int, default=0xFFF1)
parser.add_argument("--packet_size", type=any_base_int, default=20)
parser.add_argument("hex_file", help="bytes to send")
args = parser.parse_args()

print(f"=== Loading {args.hex_file} ===")
chunks = [b""]
with open(args.hex_file) as hex_file:
    for line in hex_file:
        line = line.strip().replace(":", " ")
        if not line:
            chunks.extend([b""] if chunks[-1] else [])
        elif not line.startswith("#"):
            line_data = bytes(int(w, 16) for w in line.split() if w)
            chunks[-1] = chunks[-1] + line_data

lengths = ", ".join(f"{len(c)}b" for c in chunks)
print(f"{len(chunks)} chunks ({lengths})")
print()

address = args.address
if not address:
    print('=== Scanning for "CoolLED" nametags ===')
    scanner = bluepy.btle.Scanner()
    devices = scanner.scan(timeout=2)
    nametags = [
        dev
        for dev in devices
        if any(t == 9 and v == "CoolLED" for t, d, v in dev.getScanData())
    ]

    print(f"{len(nametags)} nametag(s) found ({len(devices)} BT device(s))")
    for dev in nametags:
        m = next(v for t, d, v in dev.getScanData() if t == 255)
        print(f"  {dev.addr} ({m[:4].upper()})")
    if not nametags:
        print("*** No nametags to connect!")
        sys.exit(1)
    elif len(nametags) > 1:
        print("*** Use --address to pick one")
        sys.exit(1)

    address = nametags[0].addr
    print()

print(f"=== Connecting to {address} ===")
with bluepy.btle.Peripheral(address, iface=args.interface) as per:
    print(f"Looking up service 0x{args.service:x}...")
    service = per.getServiceByUUID(args.service)
    print(f"Looking up characteristic 0x{args.characteristic:x}...")
    characteristics = service.getCharacteristics(args.characteristic)
    if not characteristics:
        print("*** No characteristic handle found!")
        sys.exit(1)
    elif len(characteristics) > 1:
        handles = ", ".join(f"#{c.getHandle()}" for c in characteristics)
        print(f"*** Multiple handles found: {handles}")
        sys.exit(1)
    print()

    print(f"=== Sending data ===")
    char = characteristics[0]
    for chunk in chunks:
        for start in range(0, len(chunk), args.packet_size):
            packet = chunk[start : start + args.packet_size]
            print("Sending:", " ".join(f"{b:02x}" for b in packet))
            char.write(packet)

        value = char.read()
        print("Read:", " ".join(f"{b:02x}" for b in value))
    print()
