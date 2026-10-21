#!/usr/bin/env python3
"""Build Proton's CJK replacement fonts with fontTools instead of afdko.

Proton (Makefile.in, proton-11.0-2e) builds msyh.ttf, simsun.ttc (SimSun
+ NSimSun) and malgun.ttf from Source Han Sans CID sources with afdko's
makeotf, renaming them with fonts/patches/*-FontMenuNameDB, and converts
msyh and simsun to TrueType outlines; msgothic.ttc is Ume Gothic / P Gothic
/ UI Gothic with fonts/patches/Ume*-nametable.patch applied (ttx).  afdko is
not in the ports tree, so this starts from Adobe's prebuilt region OTFs
(SourceHanSansSC-Regular.otf, SourceHanSansK-Regular.otf, the faces the
FontMenuNameDB files name) and does the renaming, outline conversion and
collection building with fontTools.  As in Proton, these fonts are not the
Microsoft fonts; the names are for programs that require them.

usage: cjk-fonts.py PROTON_FONTS_DIR SC_OTF K_OTF OUTDIR
"""
import os
import re
import subprocess
import sys
import tempfile

from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.ttCollection import TTCollection

WIN = (3, 1)


def menu_names(db):
    """Parse a FontMenuNameDB: English and localized family/style names."""
    names = {"f": None, "s": None, "loc": []}
    for line in open(db, encoding="ascii"):
        m = re.match(r"\s*([fs])=(?:3,1,(0x[0-9a-fA-F]+),)?(.*)", line.rstrip("\n"))
        if not m:
            continue
        key, lang, value = m.groups()
        value = re.sub(r"\\([0-9A-Fa-f]{4})", lambda u: chr(int(u.group(1), 16)), value)
        if lang:
            names["loc"].append((key, int(lang, 16), value))
        else:
            names[key] = value
    return names


def rename(font, family, style="Regular", localized=()):
    name = font["name"]
    name.names = [n for n in name.names if n.nameID not in (1, 2, 3, 4, 6, 16, 17, 18)]
    ps = family.replace(" ", "") + ("" if style == "Regular" else "-" + style)
    full = family if style == "Regular" else family + " " + style
    for nid, value in ((1, family), (2, style), (3, ps), (4, full), (6, ps)):
        name.setName(value, nid, *WIN, 0x409)
        name.setName(value, nid, 1, 0, 0)
    for key, lang, value in localized:
        if key == "f":
            name.setName(value, 1, *WIN, lang)
            name.setName(value, 4, *WIN, lang)
        else:
            name.setName(value, 2, *WIN, lang)
    if "CFF " in font:
        font["CFF "].cff.fontNames = [ps]
        top = font["CFF "].cff.topDictIndex[0]
        for attr, value in (("FullName", full), ("FamilyName", family)):
            if hasattr(top, attr):
                setattr(top, attr, value)


def otf_to_ttf(font, max_err=1.0, reverse=True):
    """CFF -> glyf conversion (fontTools snippets/otf2ttf.py)."""
    glyph_order = font.getGlyphOrder()
    font["loca"] = newTable("loca")
    font["glyf"] = glyf = newTable("glyf")
    glyf.glyphOrder = glyph_order
    glyf.glyphs = {}
    gs = font.getGlyphSet()
    for g in glyph_order:
        pen = TTGlyphPen(gs)
        gs[g].draw(Cu2QuPen(pen, max_err, reverse_direction=reverse))
        glyf[g] = pen.glyph()
    del font["CFF "]
    if "VORG" in font:
        del font["VORG"]
    font["maxp"] = maxp = newTable("maxp")
    maxp.tableVersion = 0x00010000
    for attr in ("maxZones", "maxTwilightPoints", "maxStorage", "maxFunctionDefs",
                 "maxInstructionDefs", "maxStackElements", "maxSizeOfInstructions",
                 "maxComponentElements"):
        setattr(maxp, attr, 0)
    maxp.maxZones = 1
    font["head"].indexToLocFormat = 0
    font["head"].glyphDataFormat = 0
    font["post"].formatType = 3.0
    font.sfntVersion = "\0\1\0\0"
    font.recalcBBoxes = True


def build_source_han(fontsdir, sc_otf, k_otf, out):
    patches = os.path.join(fontsdir, "patches")

    yahei = menu_names(os.path.join(patches, "YaHei-FontMenuNameDB"))
    f = TTFont(sc_otf)
    rename(f, yahei["f"], yahei["s"], yahei["loc"])
    otf_to_ttf(f)
    f.save(os.path.join(out, "msyh.ttf"))

    # SimSun and NSimSun share everything but the names
    members = []
    for db in ("SimSun-FontMenuNameDB", "NSimSun-FontMenuNameDB"):
        n = menu_names(os.path.join(patches, db))
        f = TTFont(sc_otf)
        rename(f, n["f"], n["s"], n["loc"])
        members.append(f)
    otf_to_ttf(members[0])
    tmp = os.path.join(out, "simsun-0.ttf")
    members[0].save(tmp)
    members[0] = TTFont(tmp)
    second = TTFont(tmp)
    second["name"] = members[1]["name"]
    os.unlink(tmp)
    ttc = TTCollection()
    ttc.fonts = [members[0], second]
    ttc.save(os.path.join(out, "simsun.ttc"), shareTables=True)

    malgun = menu_names(os.path.join(patches, "Malgun-FontMenuNameDB"))
    f = TTFont(k_otf)
    rename(f, malgun["f"], malgun["s"], malgun["loc"])
    f.save(os.path.join(out, "malgun.ttf"))


def build_ume(fontsdir, out):
    ume = os.path.join(fontsdir, "ume")
    patches = os.path.join(fontsdir, "patches")
    fonts = []
    with tempfile.TemporaryDirectory() as tmp:
        for src, patch in (("ume-tgo4.ttf", "UmeGothic-nametable.patch"),
                           ("ume-pgo4.ttf", "UmePGothic-nametable.patch"),
                           ("ume-ugo4.ttf", "UmeUIGothic-nametable.patch")):
            ttx = os.path.join(tmp, patch.replace(".patch", ".ttx"))
            f = TTFont(os.path.join(ume, src))
            f.saveXML(ttx, tables=["name"])
            subprocess.run(["patch", "-s", ttx, os.path.join(patches, patch)], check=True)
            f.importXML(ttx)
            fonts.append(f)
        ttc = TTCollection()
        ttc.fonts = fonts
        ttc.save(os.path.join(out, "msgothic.ttc"))


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    fontsdir, sc_otf, k_otf, out = sys.argv[1:]
    build_source_han(fontsdir, sc_otf, k_otf, out)
    build_ume(fontsdir, out)


if __name__ == "__main__":
    main()
