# Protocol encoding for the nametag (see bluetooth.py for hardware access)

import operator
import struct
from collections.abc import Collection
from functools import reduce
from typing import List


def packets_from_data(message_type: int, data: bytes) -> List[bytes]:
    def escape123(data: bytes) -> bytes:
        data = data.replace(b"\2", b"\2\6")
        data = data.replace(b"\1", b"\2\5")
        data = data.replace(b"\3", b"\2\7")
        return data

    packets: List[bytes] = []
    for index, start in enumerate(range(0, len(data), 128)):
        chunk = data[start : start + 128]
        body = struct.pack(">xHHB", len(data), index, len(chunk)) + chunk
        body = body + struct.pack(">B", reduce(operator.xor, body, 0))
        body = struct.pack(">B", message_type) + body
        body = struct.pack(">H", len(body)) + body
        message = b"\1" + escape123(body) + b"\3"
        packets.extend(message[s : s + 20] for s in range(0, len(message), 20))

    return packets


def packets_from_glyphs(glyphs: Collection[bytes]) -> List[bytes]:
    header = struct.pack(
        ">24xB80sH",
        len(glyphs),
        bytes(len(b) for b in glyphs),
        sum(len(b) for b in glyphs),
    )
    return packets_from_data(2, header + b"".join(glyphs))
