#!/usr/bin/env python3
"""Generates the PocketBook launcher icons (8-bit BMP) for the app.
Three rising bars, outlined. Black on white; the _f variant is inverted
(the firmware's focused style).

Two independent things have to be right here:

* **The canvas must be 106x128.** The launcher blits the bitmap into a slot
  of exactly that size instead of scaling it, and silently drops an icon of
  any other size -- the tile then shows no image at all and its label slides
  up into where the icon should have been. A 48x48 canvas looks right in a
  viewer and is a perfectly valid BMP, but never appears on the device.
* **The drawing inside it should stay light.** The firmware's own icons are
  line art on a ~48 unit grid with a 2-3 px stroke, so a filled or heavy
  glyph stands out among them. The glyph is therefore laid out on a 48 unit
  grid, scaled up onto the canvas, while the stroke stays an absolute pixel
  width -- scaling the geometry without scaling the stroke is what keeps it
  as thin as the stock icons at a larger size.
"""
import argparse
import os
from PIL import Image, ImageDraw

# Canvas. The launcher lays a tile out as image-then-label using the bitmap's
# own height, so the canvas height decides where the label sits: too tall and
# the label drops below the row of stock labels next to it. Width stays at 106,
# the value known to render. Override with --canvas/--glyph when probing a
# device, but keep the default at whatever was last confirmed on one.
W, H = 106, 64
SS = 8  # supersampling for smooth edges

GRID = 48       # unit grid the glyph below is described in
GLYPH = 56      # how many canvas pixels that grid spans, ~ the stock glyph size
BAR_STROKE = 2  # outline weight in canvas pixels, deliberately not scaled

# Bars: outlined like everything else the launcher shows. The stock icons are
# pure line art, so a filled block reads as far heavier than its neighbours
# even at the right size. Sized to sit centred on the 48 unit grid: 38 wide,
# and 6..42 vertically, so the glyph's midpoint is the grid's.
BAR_W = 10
BAR_GAP = 4
BAR_HEIGHTS = [16, 26, 36]
BAR_BASE = 42

def draw_icon(fg, bg):
    k = GLYPH / GRID            # grid unit -> canvas pixel
    ox = (W - GLYPH) / 2.0      # glyph origin, centred on the canvas
    oy = (H - GLYPH) / 2.0
    im = Image.new("L", (W * SS, H * SS), bg)
    d = ImageDraw.Draw(im)

    def px(x, y):
        """Grid coordinate -> supersampled canvas coordinate."""
        return ((ox + x * k) * SS, (oy + y * k) * SS)

    def box(x0, y0, x1, y1, weight):
        d.rectangle([px(x0, y0), px(x1, y1)], outline=fg, width=weight * SS)

    # --- rising bars ---
    total = len(BAR_HEIGHTS) * BAR_W + (len(BAR_HEIGHTS) - 1) * BAR_GAP
    x = (GRID - total) / 2.0
    for bh in BAR_HEIGHTS:
        box(x, BAR_BASE - bh, x + BAR_W, BAR_BASE, BAR_STROKE)
        x += BAR_W + BAR_GAP

    im = im.resize((W, H), Image.LANCZOS)
    return im.convert("P", palette=Image.ADAPTIVE, colors=16)


def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    draw_icon(fg=0, bg=255).save(os.path.join(outdir, "pocketbook-statistics.bmp"))
    draw_icon(fg=255, bg=0).save(os.path.join(outdir, "pocketbook-statistics_f.bmp"))
    print("written:", outdir)


def parse_size(text):
    w, _, h = text.lower().partition("x")
    return int(w), int(h)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("outdir", nargs="?", default=".")
    ap.add_argument("--canvas", type=parse_size, metavar="WxH",
                    help="canvas size, e.g. 106x64 (default %dx%d)" % (W, H))
    ap.add_argument("--glyph", type=int, metavar="PX",
                    help="glyph size in canvas pixels (default %d)" % GLYPH)
    args = ap.parse_args()
    if args.canvas:
        W, H = args.canvas
    if args.glyph:
        GLYPH = args.glyph
    main(args.outdir)
