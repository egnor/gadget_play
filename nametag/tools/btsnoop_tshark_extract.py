#!/usr/bin/env python3

import argparse
import datetime
import functools
import json
import operator
import subprocess
from typing import Dict, List, Optional, Tuple

parser = argparse.ArgumentParser()
parser.add_argument("read", help="Log to parse")
args = parser.parse_args()

decode_as = "btatt.handle==0x0001-0xFFFF,btgatt.uuid0x181c"
filter_ex = 'bthci_acl.dst.name=="CoolLED" and btatt.opcode.method==0x12'
print(f"=== Decoding {args.read} ===")
tshark = subprocess.run(
    ["tshark", "-r", args.read, "-Y", filter_ex, "-d", decode_as, "-T", "json"],
    stdout=subprocess.PIPE,
    check=True,
)

basename = args.read.replace(".btsnoop", "")

target_when_data: Dict[str, List[Tuple[datetime.datetime, bytes]]] = {}
for packet in json.loads(tshark.stdout):
    layers = packet["_source"]["layers"]
    if "_ws.malformed" in layers:
        continue

    target = layers["bthci_acl"]["bthci_acl.dst.bd_addr"]
    epoch = float(layers["frame"]["frame.time_epoch"])
    when = datetime.datetime.fromtimestamp(epoch)

    hexstr = layers["btatt"]["btgatt.uuid0x181c"]["btatt.value"]
    data = bytes(int(h, 16) for h in hexstr.split(":") if h)
    target_when_data.setdefault(target, []).append((when, data))


def ftime(time):
    return time.strftime("%m-%d %H:%M:%S.") + time.strftime("%f")[:2]


for target, when_data in target_when_data.items():
    start, buf = None, b""
    for when, data in when_data:
        start, buf = (start, buf + data) if buf else (when, data)
        if len(data) == 20:
            continue

        message, buf = buf, b""

        if message[:2] != b"\x01\x00" or message[3:4] != b"\x02":
            print(f"*** {ftime(when)}: Bad start ({message[:4].hex(':')})")
            continue

        if message[-1:] != b"\x03":
            print(f"*** {ftime(when)}: Bad end ({message[-1:].hex(':')})")
            continue

        xor = functools.reduce(operator.xor, message[4:-2], 0)
        check = message[-2]
        if xor != check:
            print(
                f"*** {ftime(when)}: Bad check (len={len(message):<3d} "
                f"xor={xor:02x} check={check:02x} err={xor ^ check:02x})"
            )
            continue

        print(
            f"{ftime(start)}: Found message "
            f"(len={len(message):<3d} xor={xor:02x})"
        )

    if buf:
        print(f"*** {ftime(start)}: Partial message (len={len(buf)})")
