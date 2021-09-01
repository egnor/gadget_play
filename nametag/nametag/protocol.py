# Protocol encoding for the nametag (see bluetooth.py for hardware access)

import operator
import struct
from collections.abc import Collection, Iterable
from functools import reduce
from typing import NamedTuple, Optional, Tuple

import PIL  # type: ignore


class ProtocolStep(NamedTuple):
    send: bytes
    expect: bytes


def _chunks(*, data: bytes, size: int, expect=b"") -> Iterable[ProtocolStep]:
    yield from (
        ProtocolStep(
            send=data[s : s + size],
            expect=b"" if s + size < len(data) else expect,
        )
        for s in range(0, len(data), size)
    )


def _encode(*, tag: int, data: bytes, expect=b"") -> Iterable[ProtocolStep]:
    def escape123(data: bytes) -> bytes:
        data = data.replace(b"\2", b"\2\6")
        data = data.replace(b"\1", b"\2\5")
        data = data.replace(b"\3", b"\2\7")
        return data

    typed = struct.pack(">B", tag) + data
    sized_typed = struct.pack(">H", len(typed)) + typed
    escaped_sized_typed = b"\1" + escape123(sized_typed) + b"\3"
    return _chunks(data=escaped_sized_typed, size=20, expect=expect)


def _encode_chunked(*, tag: int, data: bytes) -> Iterable[ProtocolStep]:
    for index, (chunk, expect) in enumerate(_chunks(data=data, size=128)):
        body = struct.pack(">xHHB", len(data), index, len(chunk)) + chunk
        body = body + struct.pack(">B", reduce(operator.xor, body, 0))
        exp = next(iter(_encode(tag=tag, data=struct.pack(">xHx", index))))[0]
        yield from _encode(tag=tag, data=body, expect=exp)


def show_glyphs(glyphs: Collection[PIL.Image.Image]) -> Iterable[ProtocolStep]:
    if not glyphs:
        raise ValueError("No glyphs to show")
    for i, glyph in enumerate(glyphs):
        if glyph.mode != "1":
            raise ValueError(f'Image mode "{glyph.mode}" instead of "1"')
        if glyph.size[1] > 48 or glyph.size[1] != 12:
            raise ValueError(f"Image size {glyph.size} != ([1-48], 12)")

    as_bytes = [g.transpose(PIL.Image.TRANSPOSE).tobytes() for g in glyphs]
    header = struct.pack(
        ">24xB80sH",
        len(as_bytes),
        bytes(len(b) for b in as_bytes),
        sum(len(b) for b in as_bytes),
    )
    return _encode_chunked(tag=2, data=header + b"".join(as_bytes))


def show_frames(
    frames: Collection[PIL.Image.Image], *, msec=250
) -> Iterable[ProtocolStep]:
    if not frames:
        raise ValueError("No frames to show")
    for i, f in enumerate(frames):
        if f.size != (48, 12):
            raise ValueError(f"Frame #{i} size {f.size()} != (48, 12)")

    header = struct.pack(">24xBH", len(frames), msec)
    body = b"".join(f.transpose(PIL.Image.TRANSPOSE).tobytes() for f in frames)
    return _encode_chunked(tag=4, data=header + body)


def set_mode(mode) -> Iterable[ProtocolStep]:
    return _encode(tag=6, data=struct.pack(">B", mode))


def set_speed(speed) -> Iterable[ProtocolStep]:
    return _encode(tag=7, data=struct.pack(">B", speed))


def set_brightness(brightness) -> Iterable[ProtocolStep]:
    return _encode(tag=8, data=struct.pack(">B", brightness))
