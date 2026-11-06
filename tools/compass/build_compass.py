r"""Builds the Nordic compass art the DLL loads (DEM 1.9.0).

    py -3 tools/compass/build_compass.py [--render] [--previews <dir>]

--render      runs Blender on compass_scene.py first (found on PATH or under Program Files\Blender Foundation),
              rendering 1024 px layers into tools/compass/render/
Then: each layer is reduced to 512 px (tools/compass/layers/<layer>.png, the committed sources) and written into
dist/Interface/DragonsEyeMinimap/compass.swf.

Why a SWF and not DDS: the compass is drawn in Skyrim's HUD movie (Scaleform), and Scaleform cannot load a loose DDS
(`img://` only finds images the game registered; past-mistakes library, LMU 1.3.3). A SWF loaded with loadMovie is the
route HUD Position Manager's widgets already prove. The bitmaps are embedded lossless (DefineBitsLossless2, premultiplied
ARGB) and filled into a square shape with a SMOOTHED clipped bitmap fill, so a rotated layer stays clean.

The SWF's main timeline holds four named sprites, each centred on (0, 0) and 512 px across (the compass's outer edge is
radius 0.98 of the half-width, 251 px): body, rose, needle, cap - stacked in that order. The DLL sets rose._rotation and
needle._rotation and scales the whole movie to the minimap's circle. A reskin keeps the names and replaces the art.

--previews <dir> writes each layer plus two composites (north up; turned 45 degrees with the needle elsewhere) there.
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import zlib

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import swfgen  # noqa: E402  (HUD Position Manager's writer, vendored)

REPO = os.path.dirname(os.path.dirname(HERE))
LAYERS = ["body", "rose", "needle", "cap"]
SIZE = 512
OUTER_FRACTION = 0.98   # outer edge radius as a fraction of the half-width; the DLL uses the same number


def find_blender():
    exe = shutil.which("blender")
    if exe:
        return exe
    root = os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "Blender Foundation")
    if os.path.isdir(root):
        for d in sorted(os.listdir(root), reverse=True):
            cand = os.path.join(root, d, "blender.exe")
            if os.path.isfile(cand):
                return cand
    sys.exit("Blender not found: install it (winget install BlenderFoundation.Blender) or put it on PATH")


def render():
    out = os.path.join(HERE, "render")
    os.makedirs(out, exist_ok=True)
    cmd = [find_blender(), "--background", "--factory-startup", "--python", os.path.join(HERE, "compass_scene.py"), "--", out, "1024"]
    print("render:", " ".join(cmd))
    subprocess.run(cmd, check=True)


def reduce_layers():
    src = os.path.join(HERE, "render")
    dst = os.path.join(HERE, "layers")
    os.makedirs(dst, exist_ok=True)
    for name in LAYERS:
        p = os.path.join(src, name + ".png")
        if not os.path.isfile(p):
            continue
        img = Image.open(p).convert("RGBA")
        # premultiply before resampling so transparent edges carry no colour fringe, then back
        img = img.convert("RGBa").resize((SIZE, SIZE), Image.LANCZOS).convert("RGBA")
        img.save(os.path.join(dst, name + ".png"), optimize=True)
        print(f"layer {name}: {SIZE}px")


def bits_lossless2(char_id, img):
    """DefineBitsLossless2: 32-bit premultiplied ARGB, zlib."""
    w, h = img.size
    pm = img.convert("RGBa").tobytes()            # premultiplied RGBA
    argb = bytearray(len(pm))
    argb[0::4] = pm[3::4]
    argb[1::4] = pm[0::4]
    argb[2::4] = pm[1::4]
    argb[3::4] = pm[2::4]
    body = struct.pack("<HBHH", char_id, 5, w, h) + zlib.compress(bytes(argb), 9)
    return swfgen.tag(36, body)


def bitmap_square(char_id, bitmap_id, size):
    """DefineShape3: a size x size square centred on (0, 0) filled with the bitmap (0x41: clipped, smoothed)."""
    half = size / 2
    body = bytearray(struct.pack("<H", char_id)) + swfgen.rect(-half, half, -half, half)
    body.append(1)                                            # one fill style
    body += b"\x41" + struct.pack("<H", bitmap_id)
    body += swfgen.matrix(tx=-half, ty=-half, sx=20.0, sy=20.0)   # bitmap px -> twips, top-left at (-half, -half)
    body.append(0)                                            # no line styles
    b = swfgen.Bits()
    b.u(1, 4)                                                 # fill bits
    b.u(0, 4)                                                 # line bits
    pts = [(-half, -half), (half, -half), (half, half), (-half, half)]
    x0, y0 = int(round(pts[0][0] * 20)), int(round(pts[0][1] * 20))
    b.u(0, 1); b.u(0, 1); b.u(0, 1); b.u(1, 1); b.u(0, 1); b.u(1, 1)   # style change: fill style 1, move to
    n = swfgen.nbits_signed(x0, y0)
    b.u(n, 5); b.s(x0, n); b.s(y0, n)
    b.u(1, 1)                                                 # fill style 1 = the bitmap
    cx, cy = x0, y0
    for x, y in pts[1:] + [pts[0]]:
        tx, ty = int(round(x * 20)), int(round(y * 20))
        dx, dy = tx - cx, ty - cy
        n = max(2, swfgen.nbits_signed(dx, dy))
        b.u(1, 1); b.u(1, 1); b.u(n - 2, 4)
        if dx and dy:
            b.u(1, 1); b.s(dx, n); b.s(dy, n)
        elif dx:
            b.u(0, 1); b.u(0, 1); b.s(dx, n)
        else:
            b.u(0, 1); b.u(1, 1); b.s(dy, n)
        cx, cy = tx, ty
    b.u(0, 6)
    body += b.bytes()
    return swfgen.tag(32, bytes(body))


def build_swf():
    tags = [struct.pack("<H", (9 << 6) | 3) + b"\x00\x00\x00"]   # SetBackgroundColor black (ignored when loaded)
    cid = 1
    for depth, name in enumerate(LAYERS, start=1):
        img = Image.open(os.path.join(HERE, "layers", name + ".png")).convert("RGBA")
        bmp, shp, spr = cid, cid + 1, cid + 2
        cid += 3
        tags.append(bits_lossless2(bmp, img))
        tags.append(bitmap_square(shp, bmp, img.size[0]))
        tags.append(swfgen.sprite(spr, [(1, shp, None, None)]))
        tags.append(swfgen.place(depth, spr, name, swfgen.matrix()))
    data = swfgen.movie(SIZE, SIZE, tags)
    out_dir = os.path.join(REPO, "dist", "Interface", "DragonsEyeMinimap")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "compass.swf")
    with open(out, "wb") as f:
        f.write(data)
    print(f"swf: {out} ({len(data) // 1024} KB)")


def previews(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    L = {n: Image.open(os.path.join(HERE, "layers", n + ".png")).convert("RGBA") for n in LAYERS}
    for n, img in L.items():
        img.save(os.path.join(out_dir, f"layer-{n}.png"))

    def backdrop():
        # a snowy, overcast tundra stand-in so the art is judged against something like the game
        bg = Image.new("RGBA", (SIZE, SIZE))
        px = bg.load()
        for y in range(SIZE):
            for x in range(SIZE):
                t = (x + y) / (2 * SIZE)
                px[x, y] = (int(120 + 40 * t), int(128 + 38 * t), int(134 + 36 * t), 255)
        return bg

    def compose(heading_deg, needle_screen_deg):
        c = backdrop()
        for n in LAYERS:
            img = L[n]
            if n == "rose":
                img = img.rotate(heading_deg, resample=Image.BICUBIC)         # card turns AGAINST the heading
            elif n == "needle":
                img = img.rotate(-needle_screen_deg, resample=Image.BICUBIC)  # clockwise screen angle
            c = Image.alpha_composite(c, img)
        return c

    north = compose(0, 0)
    north.save(os.path.join(out_dir, "composite-north-up.png"))
    turned = compose(45, 120 - 45)   # camera heading 45 (NE), objective bearing 120 (ESE)
    turned.save(os.path.join(out_dir, "composite-heading45-quest120.png"))
    sheet = Image.new("RGBA", (SIZE * 2 + 3 * 180 + 40, SIZE), (24, 26, 30, 255))
    sheet.alpha_composite(north, (0, 0))
    sheet.alpha_composite(turned, (SIZE, 0))
    for i, img in enumerate((north, turned)):
        small = img.resize((180, 180), Image.LANCZOS)   # about the size it takes on a 1440p HUD
        sheet.alpha_composite(small, (SIZE * 2 + 20 + i * 190, 20))
    sheet.save(os.path.join(out_dir, "sheet.png"))
    print(f"previews: {out_dir}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--previews")
    a = ap.parse_args()
    if a.render:
        render()
    reduce_layers()
    build_swf()
    if a.previews:
        previews(a.previews)
