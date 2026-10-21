#!/usr/bin/env python3
"""Convert an X11 xwd dump (ZPixmap, 24/32 bpp, as `xwd -root` on Xvfb
writes) to PNG, without ImageMagick's XWD delegate.

usage: xwd2png.py in.xwd out.png
"""
import struct
import sys
import zlib


def main():
    data = open(sys.argv[1], "rb").read()
    (hsize, version, fmt, depth, width, height, xoff, byte_order, unit,
     bit_order, pad, bpp, bpl, visual_class, rmask, gmask, bmask, bits_rgb,
     ncolors_map, ncolors) = struct.unpack(">20I", data[:80])
    if version != 7 or fmt != 2 or bpp not in (24, 32):
        sys.exit(f"unsupported xwd: version {version} format {fmt} bpp {bpp}")
    pixels = data[hsize + ncolors * 12:]
    step = bpp // 8
    order = "<" if byte_order == 0 else ">"
    shift = {m: (m & -m).bit_length() - 1 for m in (rmask, gmask, bmask)}
    rows = []
    for y in range(height):
        line = pixels[y * bpl:y * bpl + width * step]
        out = bytearray(b"\0")
        for x in range(width):
            px = line[x * step:(x + 1) * step]
            v = int.from_bytes(px, "little" if order == "<" else "big")
            out += bytes(((v & m) >> shift[m]) & 0xff for m in (rmask, gmask, bmask))
        rows.append(bytes(out))

    def chunk(tag, body):
        c = tag + body
        return struct.pack(">I", len(body)) + c + struct.pack(">I", zlib.crc32(c))

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(b"".join(rows), 6)) +
           chunk(b"IEND", b""))
    open(sys.argv[2], "wb").write(png)


if __name__ == "__main__":
    main()
