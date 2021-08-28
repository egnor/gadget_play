#!/usr/bin/env python3

import argparse

import bluepy.btle  # type: ignore

parser = argparse.ArgumentParser()
parser.add_argument("--interface", type=int, default=0, help="HCI interface")
parser.add_argument("--time", type=int, default=10, help="Scan seconds")
mac_group = parser.add_mutually_exclusive_group()
mac_group.add_argument("--public", help="Public-type MAC to probe")
mac_group.add_argument("--random", help="Random-type MAC to probe")
args = parser.parse_args()

if args.public:
    print(f"Connecting to public={args.public}...")
    per = bluepy.btle.Peripheral(
        args.public, addrType=bluepy.btle.ADDR_TYPE_PUBLIC, iface=args.interface
    )
elif args.random:
    print(f"Connecting to random={args.random}...")
    per = bluepy.btle.Peripheral(
        args.random, addrType=bluepy.btle.ADDR_TYPE_RANDOM, iface=args.interface
    )
else:
    per = None

if per:
    print("Discovering services...")
    services = per.getServices()
    print(f"Found {len(services)} services:")
    for service in services:
        print(f"=== {service.uuid.getCommonName()} ===")
        for char in service.getCharacteristics():
            print(
                f"#{char.getHandle()}: {char.uuid.getCommonName()}"
                f" ({char.propertiesToString().strip()})"
            )
            if char.supportsRead():
                data = char.read()
                as_hex = data.hex(" ")
                as_text = "".join(
                    f"{ch}  " if ch.isprintable() else "?? "
                    for ch in data.decode("ascii", "replace")
                )
                print(f"    {as_hex}\n   [{as_text.rstrip()}]")
        print()

else:
    print("Creating Scanner...")
    scanner = bluepy.btle.Scanner(iface=args.interface)

    print(f"Starting scan ({args.time}sec)...")
    devices = scanner.scan(timeout=args.time)

    print(f"Found {len(devices)} devices:")
    for dev in devices:
        print(
            f"=== {dev.addrType}={dev.addr} @hci{dev.iface}"
            f" {'' if dev.connectable else '!'}conn"
            f" rssi={dev.rssi} count={dev.updateCount} ==="
        )
        for adtype, description, value in dev.getScanData():
            print(f"[{adtype}] {description}: {value}")

        print()
