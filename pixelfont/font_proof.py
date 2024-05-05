#!/usr/bin/env python

import dataclasses
import imageio.v3 as iio
import numpy
import urllib.parse

@dataclasses.dataclass
class FontSheet:
    url: str
    offset: tuple[int, int]
    spacing: tuple[int, int]
    zoom: int
    layout: list[str]

SITE = "https://img.itch.zone"

LAYOUT = [
  "ABCDEFGH", "IJKLMNOP", "QRSTUVWX", "YZ",
  "abcdefgh", "ijklmnop", "qrstuvwx", "yz",
  "01234567", "89",
  "_!\"#$%&'", "()*+,-./", ":;<=>?[]", "\\^_`{}|~", "@",
]

SHEETS = {
  "5": FontSheet(
    url=f"{SITE}/aW1hZ2UvODU2NjU5LzE1OTc0Mjk1LnBuZw==/original/sKUkqt.png",
    offset=(648, 57),
    spacing=(18, 18),
    zoom=3,
    layout=LAYOUT,
  ),
  "7": FontSheet(
    url=f"{SITE}/aW1hZ2UvODU2NjU5LzE1OTc0MzAzLnBuZw==/original/a207oZ.png",
    offset=(352, 17),
    spacing=(8, 8),
    zoom=1,
    layout=LAYOUT,
  ),
  "9": FontSheet(
    url=f"{SITE}/aW1hZ2UvODU2NjU5LzE1OTc0MzA0LnBuZw==/original/CTuPZD.png",
    offset=(270, 63),
    spacing=(30, 30),
    zoom=3,
    layout=LAYOUT,
  ),
  "11": FontSheet(
    url=f"{SITE}/aW1hZ2UvODU2NjU5LzE2MDE3MTAzLnBuZw==/original/d1rDfz.png",
    offset=(972, 75),
    spacing=(36, 36),
    zoom=3,
    layout=LAYOUT,
  ),
}

def load_fonts():
    fonts = {}
    for name, sheet in SHEETS.items():
        url = urllib.parse.urlsplit(sheet.url)
        print(f"📥 \"{name}\" {url.path.split('/')[-1]}")
        im = (iio.imread(url.geturl()).mean(axis=2) < 128)

        ox, oy = sheet.offset
        cw, ch = sheet.spacing
        z = sheet.zoom

        glyphs = fonts.setdefault(name, {})
        for cell_y, row in enumerate(LAYOUT):
            for cell_x, char in enumerate(row):
                x = ox + cell_x * cw
                y = oy + cell_y * ch
                cell = im[y:y + ch, x:x + cw][::z, ::z]
                bbw = numpy.argmax(cell.any(axis=0)[::-1])
                bbh = numpy.argmax(cell.any(axis=1)[::-1])
                glyphs[char] = cell[:bbh, :bbw]

        maxw = max(len(g) for g in glyphs.values())
        maxh = max(len(g[0]) for g in glyphs.values() if len(g))
        print(f"  🔠 {len(glyphs)} glyphs, max {maxw}x{maxh}")
        print()


def main():
    fonts = load_fonts()


if __name__ == "__main__":
    main()
