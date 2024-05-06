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
    unzoom: int
    layout: list[str]

@dataclasses.dataclass
class Font:
    name: str
    glyphs: dict[str, numpy.ndarray]

SITE = "https://img.itch.zone"

LAYOUT = [
  "ABCDEFGH", "IJKLMNOP", "QRSTUVWX", "YZ",
  "abcdefgh", "ijklmnop", "qrstuvwx", "yz",
  "01234567", "89",
  "_!\"#$%&'", "()*+,-./", ":;<=>?[]", "\\^_`{}|~", "@",
]

SHEETS = {
  "5px": FontSheet(
    url=f"{SITE}/aW1hZ2UvODU2NjU5LzE1OTc0Mjk1LnBuZw==/original/sKUkqt.png",
    offset=(648, 57),
    spacing=(18, 18),
    unzoom=3,
    layout=LAYOUT,
  ),
  "7px": FontSheet(
    url=f"{SITE}/aW1hZ2UvODU2NjU5LzE1OTc0MzAzLnBuZw==/original/a207oZ.png",
    offset=(352, 17),
    spacing=(8, 8),
    unzoom=1,
    layout=LAYOUT,
  ),
  "9px": FontSheet(
    url=f"{SITE}/aW1hZ2UvODU2NjU5LzE1OTc0MzA0LnBuZw==/original/CTuPZD.png",
    offset=(270, 60),
    spacing=(30, 30),
    unzoom=3,
    layout=LAYOUT,
  ),
  "11px": FontSheet(
    url=f"{SITE}/aW1hZ2UvODU2NjU5LzE2MDE3MTAzLnBuZw==/original/d1rDfz.png",
    offset=(972, 75),
    spacing=(36, 36),
    unzoom=3,
    layout=LAYOUT,
  ),
}

HOEFLER_PROOF = """
Angel Adept Blind Bodice Clique Coast Dunce Docile Enact Eosin Furlong Focal Gnome Gondola Human Hoist Inlet Iodine Justin Jocose Knoll Koala Linden Loads Milliner Modal Number Nodule Onset Oddball Pneumo Poncho Quanta Qophs Rhone Roman Snout Sodium Tundra Tocsin Uncle Udder Vulcan Vocal Whale Woman Xmas Xenon Yunnan Young Zloty Zodiac.

Angel angel adept for the nuance loads of the arena cocoa and quaalude.
Blind blind bodice for the submit oboe of the club snob and abbot.
Clique clique coast for the pouch loco of the franc assoc and accede.
Dunce dunce docile for the loudness mastodon of the loud statehood and huddle.
Enact enact eosin for the quench coed of the pique canoe and bleep.
Furlong furlong focal for the genuflect profound of the motif aloof and offers.
Gnome gnome gondola for the impugn logos of the unplug analog and smuggle.
Human human hoist for the buddhist alcohol of the riyadh caliph and bathhouse.
Inlet inlet iodine for the quince champion of the ennui scampi and shiite.
Justin justin jocose for the djibouti sojourn of the oranj raj and hajjis.
Knoll knoll koala for the banknote lookout of the dybbuk outlook and trekked.
Linden linden loads for the ulna monolog of the consul menthol and shallot.
Milliner milliner modal for the alumna solomon of the album custom and summon.
Number number nodule for the unmade economic of the shotgun bison and tunnel.
Onset onset oddball for the abandon podium of the antiquo tempo and moonlit.
Pneumo pneumo poncho for the dauphin opossum of the holdup bishop and supplies.
Quanta quanta qophs for the inquest sheqel of the cinq coq and suqqu.
Rhone rhone roman for the burnt porous of the lemur clamor and carrot.
Snout snout sodium for the ensnare bosom of the genus pathos and missing.
Tundra tundra tocsin for the nutmeg isotope of the peasant ingot and ottoman.
Uncle uncle udder for the dunes cloud of the hindu thou and continuum.
Vulcan vulcan vocal for the alluvial ovoid of the yugoslav chekhov and revved.
Whale whale woman for the meanwhile blowout of the forepaw meadow and glowworm.
Xmas xmas xenon for the bauxite doxology of the tableaux equinox and exxon.
Yunnan yunnan young for the dynamo coyote of the obloquy employ and sayyid.
Zloty zloty zodiac for the gizmo ozone of the franz laissez and buzzing.

ABIDE ACORN OF THE HABIT DACRON FOR THE BUDDHA GOUDA QUAALUDE.
BENCH BOGUS OF THE SCRIBE ROBOT FOR THE APLOMB JACOB RIBBON.
CENSUS CORAL OF THE SPICED JOCOSE FOR THE BASIC HAVOC SOCCER.
DEMURE DOCILE OF THE TIDBIT LODGER FOR THE CUSPID PERIOD BIDDER.
EBBING ECHOING OF THE BUSHED DECAL FOR THE APACHE ANODE NEEDS.
FEEDER FOCUS OF THE LIFER BEDFORD FOR THE SERIF PROOF BUFFER.
GENDER GOSPEL OF THE PIGEON DOGCART FOR THE SPRIG QUAHOG DIGGER.
HERALD HONORS OF THE DIHEDRAL MADHOUSE FOR THE PENH RIYADH BATHHOUSE.
IBSEN ICEMAN OF THE APHID NORDIC FOR THE SUSHI SAUDI SHIITE.
JENNIES JOGGER OF THE TIJERA ADJOURN FOR THE ORANJ KOWBOJ HAJJIS.
KEEPER KOSHER OF THE SHRIKE BOOKCASE FOR THE SHEIK LOGBOOK CHUKKAS.
LENDER LOCKER OF THE CHILD GIGOLO FOR THE UNCOIL GAMBOL ENROLLED.
MENACE MCCOY OF THE NIMBLE TOMCAT FOR THE DENIM RANDOM SUMMON.
NEBULA NOSHED OF THE INBRED BRONCO FOR THE COUSIN CARBON KENNEL.
OBSESS OCEAN OF THE PHOBIC DOCKSIDE FOR THE GAUCHO LIBIDO HOODED.
PENNIES PODIUM OF THE SNIPER OPCODE FOR THE SCRIP BISHOP HOPPER.
QUANTA QOPHS OF THE INQUEST OQOS FOR THE CINQ COQ SUQQU.
REDUCE ROGUE OF THE GIRDLE ORCHID FOR THE MEMOIR SENSOR SORREL.
SENIOR SCONCE OF THE DISBAR GODSON FOR THE HUBRIS AMENDS LESSEN.
TENDON TORQUE OF THE UNITED SCOTCH FOR THE NOUGHT FORGOT BITTERS.
UNDER UGLINESS OF THE RHUBARB SEDUCE FOR THE MANCHU HINDU CONTINUUM.
VERSED VOUCH OF THE DIVER OVOID FOR THE TELAVIV KARPOV FLIVVER.
WENCH WORKER OF THE UNWED SNOWCAP FOR THE ANDREW ESCROW GLOWWORM.
XENON XOCHITL OF THE MIXED BOXCAR FOR THE SUFFIX ICEBOX EXXON.
YEOMAN YONDER OF THE HYBRID ARROYO FOR THE DINGHY BRANDY SAYYID.
ZEBRA ZOMBIE OF THE PRIZED OZONE FOR THE FRANZ ARROZ BUZZING.
""".strip()

PANGRAM_PROOF = """
How quickly DAFT jumping ZEBRAS vex!
""".strip()

PUNCT_PROOF = """
To my "friends" & "family" (don't worry?): "Hello, world!" [me@example.com]
https://example.com:8080/index.html?q=Hello%2C%20world%21#top
C:\Program Files (x86)\Steam\steam.exe

email_rx = re.compile(r"^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}$")

int main(int argc, char *argv[]) {
  printf("Hello, world!\\n");
  return 0;
}

Markdown *bold*, _italic_, and `monospace`. 1234567890 < 9876543210 > 0x12345678
""".strip()

def load_fonts():
    fonts = {}
    for name, sheet in SHEETS.items():
        url = urllib.parse.urlsplit(sheet.url)
        print(f"📥 \"{name}\" {url.path.split('/')[-1]}")
        im = (iio.imread(url.geturl()).mean(axis=2) < 128)

        off_x, off_y = sheet.offset
        cell_w, cell_h = sheet.spacing
        z = sheet.unzoom

        glyphs = {}
        for cell_y, row in enumerate(LAYOUT):
            for cell_x, ch in enumerate(row):
                x = off_x + cell_x * cell_w
                y = off_y + cell_y * cell_h
                cell = im[y:y + cell_h, x:x + cell_w][::z, ::z]
                nzy, nzx = cell.nonzero()
                if len(nzy) and len(nzx):
                    glyphs[ch] = cell[:nzy.max() + 1, :nzx.max() + 1]

        max_w = max(g.shape[1] for g in glyphs.values())
        max_h = max(g.shape[0] for g in glyphs.values())
        if not glyphs.get(" "):
            glyphs[" "] = numpy.zeros((max_h, (max_w + 3) // 4), dtype=bool)

        fonts[name] = Font(name, glyphs)
        print(f"  🔠 {len(glyphs)} glyphs, max {max_w}x{max_h}")
        print()

    return fonts


def text_block(font, width, text):
    en_w = font.glyphs["n"].shape[1]
    gap_w = (en_w // 6) + 1

    max_h = max(g.shape[0] for g in font.glyphs.values())
    gap_h = (max_h // 10) + 1
    row_h = max_h + gap_h

    rows = []
    for input in text.split("\n"):
        break_i, next_i, row_w = 0, 0, 0
        while next_i < len(input):
            ch = input[next_i]
            if ch.isspace():
                break_i = next_i
            ch_w = font.glyphs[ch].shape[1] + gap_w
            if row_w + ch_w > width and (break_i or next_i):
                break_i = break_i or next_i
                rows.append(input[:break_i])
                input = input[break_i:].lstrip()
                break_i, next_i, row_w = 0, 0, 0
            else:
                row_w += ch_w
                next_i += 1

        rows.append(input)

    block = numpy.zeros((len(rows) * row_h, width), dtype=bool)
    next_x, next_y = 0, (gap_h // 2)
    for row in rows:
        for ch in row:
            glyph = font.glyphs[ch]
            glyph_h, glyph_w = glyph.shape
            block[next_y:next_y + glyph_h, next_x:next_x + glyph_w] |= glyph
            next_x += glyph_w + gap_w
        next_x = 0
        next_y += row_h

    print(f"📄 \"{font.name}\" {block.shape[1]}x{block.shape[0]}")
    for row in rows:
        print(f"  {row}")
    print()
    return block


def zoom_pixels(im, z):
    return numpy.repeat(numpy.repeat(im, z, axis=0), z, axis=1)


def main():
    fonts = load_fonts()
    for name, font in fonts.items():
        width = 120 * font.glyphs["x"].shape[1]
        proof = text_block(font, width, f"{HOEFLER_PROOF}\n\n{PUNCT_PROOF}")
        iio.imwrite(f"proof_{name}.png", zoom_pixels(proof, 3))

    width = 200
    blocks = []
    for name, font in fonts.items():
        blocks.append(text_block(font, width, f"{name}: {PANGRAM_PROOF}"))
        blocks.append(numpy.zeros((5, width), dtype=bool))

    proof = numpy.concatenate(blocks[:-1], axis=0)
    iio.imwrite("proof_all.png", zoom_pixels(proof, 3))


if __name__ == "__main__":
    main()
