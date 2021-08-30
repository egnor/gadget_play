# Protocol encoding for the nametag (see bluetooth.py for hardware access)

import operator
import struct
from collections.abc import Collection, Iterable
from functools import reduce
from typing import Optional, Tuple

SendExpectPairs = Iterable[Tuple[bytes, bytes]]


def chunks(*, data: bytes, size: int, expect=b"") -> SendExpectPairs:
    yield from (
        (data[s : s + size], b"" if s + size < len(data) else expect)
        for s in range(0, len(data), size)
    )


def encode(*, tag: int, data: bytes, expect=b"") -> SendExpectPairs:
    def escape123(data: bytes) -> bytes:
        data = data.replace(b"\2", b"\2\6")
        data = data.replace(b"\1", b"\2\5")
        data = data.replace(b"\3", b"\2\7")
        return data

    typed = struct.pack(">B", tag) + data
    sized_typed = struct.pack(">H", len(typed)) + typed
    escaped_sized_typed = b"\1" + escape123(sized_typed) + b"\3"
    return chunks(data=escaped_sized_typed, size=20, expect=expect)


def encode_chunked(*, tag: int, data: bytes) -> SendExpectPairs:
    for index, (chunk, expect) in enumerate(chunks(data=data, size=128)):
        body = struct.pack(">xHHB", len(data), index, len(chunk)) + chunk
        body = body + struct.pack(">B", reduce(operator.xor, body, 0))
        expect = next(iter(encode(tag=tag, data=struct.pack(">xHx", index))))[0]
        yield from encode(tag=tag, data=body, expect=expect)


def show_glyphs(glyphs: Collection[bytes]) -> SendExpectPairs:
    header = struct.pack(
        ">24xB80sH",
        len(glyphs),
        bytes(len(b) for b in glyphs),
        sum(len(b) for b in glyphs),
    )
    return encode_chunked(tag=2, data=header + b"".join(glyphs))


def set_mode(mode) -> SendExpectPairs:
    return encode(tag=6, data=struct.pack(">B", mode))


def set_speed(speed) -> SendExpectPairs:
    return encode(tag=7, data=struct.pack(">B", speed))


def set_brightness(brightness) -> SendExpectPairs:
    return encode(tag=8, data=struct.pack(">B", brightness))
