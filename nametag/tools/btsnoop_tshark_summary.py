#!/usr/bin/env python3

import argparse
import datetime
import json
import subprocess

methods = {
    0x01: "ERROR RESP",
    0x02: "EXCH MTU",
    0x03: "EXCH MTU RESP",
    0x04: "FIND INFO",
    0x05: "FIND INFO RESP",
    0x06: "FIND BY TYPE",
    0x07: "FIND BY TYPE RESP",
    0x08: "READ BY TYPE",
    0x09: "READ BY TYPE RESP",
    0x0C: "READ BLOB",
    0x0D: "READ BLOB RESP",
    0x0E: "READ MULTI",
    0x0F: "READ MULTI RESP",
    0x0A: "READ",
    0x0B: "READ RESP",
    0x10: "READ BY GROUP",
    0x11: "READ BY GROUP RESP",
    0x12: "WRITE",
    0x13: "WRITE RESP",
    0x16: "PREP WRITE",
    0x17: "PREP WRITE RESP",
    0x18: "EXEC WRITE",
    0x19: "EXEC WRITE RESP",
    0x1B: "NOTIFY",
    0x1D: "INDICATE",
    0x1E: "CONFIRM",
    0x20: "UNSUPPORTED METHOD",
    0x52: "WRITE (NO RESP)",
    0xD2: "SIGNED WRITE (NO RESP)",
}

parser = argparse.ArgumentParser()
parser.add_argument("read", help="Log to parse")
args = parser.parse_args()

decode_as = "btatt.handle==0x0001-0xFFFF,btgatt.uuid0x181c"
tshark = subprocess.run(
    ["tshark", "-r", args.read, "-Y", "btatt", "-d", decode_as, "-T", "json"],
    stdout=subprocess.PIPE,
    check=True,
)

for packet in json.loads(tshark.stdout):
    layers = packet["_source"]["layers"]

    def lookup(path, decode=lambda x: int(x, 0), json=layers):
        for part in path.split("/"):
            json = json.get(part, {}) if isinstance(json, dict) else None
        return decode(json) if json else None

    if "_ws.malformed" in layers:
        continue

    epoch = lookup("frame/frame.time_epoch", decode=float)
    time = datetime.datetime.fromtimestamp(epoch)

    dir = lookup("hci_h4/hci_h4.direction")
    if dir == 0:  # Sent
        addr = lookup("bthci_acl/bthci_acl.dst.bd_addr", decode=str)
        name = lookup("bthci_acl/bthci_acl.dst.name", decode=str)
    elif dir == 1:  # Received
        addr = lookup("bthci_acl/bthci_acl.src.bd_addr", decode=str)
        name = lookup("bthci_acl/bthci_acl.src.name", decode=str)
    else:
        raise ValueError(f"Unknown hci_h4.direction: {dir}")

    method = lookup("btatt/btatt.opcode_tree/btatt.opcode.method")
    handle = lookup("btatt/btatt.handle")
    serv16 = lookup("btatt/btatt.handle_tree/btatt.service_uuid16")
    char16 = lookup("btatt/btatt.handle_tree/btatt.uuid16")
    data = lookup("btatt/btgatt.uuid0x181c/btatt.value", decode=str) or ""

    time_f = time.strftime("%m-%d %H:%M:%S.") + time.strftime("%f")[:2]
    dir_f = " >" if dir else "<="
    name_f = ' "' + name.replace('"', "'") + '"' if name else ""
    method_f = methods.get(method, f"[BAD METHOD {method}]")
    handle_f = f"#{handle}" if handle else ""
    serv_f = f":{serv16:04x}" if serv16 else ""
    char_f = f"/{char16:04x}" if char16 else ""
    var_f = f" [{handle_f}{serv_f}{char_f}]" if handle_f else ""
    print(f"{time_f}: {addr}{name_f} {dir_f}{var_f} {method_f} {data}")
