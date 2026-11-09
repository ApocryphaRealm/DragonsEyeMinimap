r"""Builds the Nordic compass art the DLL loads (DEM 1.9.0).

    py -3 tools/compass/build_compass.py [--render [--only rose,north]] [--previews <dir>]

--render      runs Blender on compass_scene.py first (found on PATH or under Program Files\Blender Foundation),
              rendering 1024 px layers into tools/compass/render/ (--only: just those layers; Blender runs at
              below-normal priority on half the cores, so a render can run while the game is open)
Then: each layer is reduced to 512 px (tools/compass/layers/<layer>.png, the committed sources) and written into
dist/Interface/DragonsEyeMinimap/compass.swf.

Why a SWF and not DDS: the compass is drawn in Skyrim's HUD movie (Scaleform), and Scaleform cannot load a loose DDS
(`img://` only finds images the game registered; past-mistakes library, LMU 1.3.3). A SWF loaded with loadMovie is the
route HUD Position Manager's widgets already prove. The bitmaps are embedded lossless (DefineBitsLossless2, premultiplied
ARGB) and filled into a square shape with a SMOOTHED clipped bitmap fill, so a rotated layer stays clean.

The SWF's main timeline holds four named sprites, each centred on (0, 0) and 512 px across (the compass's outer edge is
radius 0.98 of the half-width, 251 px): body, rose, needle, cap - stacked in that order. The DLL sets rose._rotation
(against the heading: the card's N points at world north) and needle._rotation (the red arm, to the quest bearing) and
scales the whole movie to the minimap's circle. A reskin keeps the names and replaces the art.

--check measures the layers: the needle's largest radius (any pixel with alpha > 8, shadow included) against the braided
cord's outer edge on the rose at its narrowest, with the needle rotated to 0, 45 and 90 degrees; and how much of the gilt
N the needle covers at every whole-degree angle. It exits 1 if the needle reaches past the cord or covers the N.

--previews <dir> writes each layer plus composites at headings 0, 45, 90 and 215 (objective bearing 120) there.
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


def render(only=""):
    out = os.path.join(HERE, "render")
    os.makedirs(out, exist_ok=True)
    threads = str(max(1, (os.cpu_count() or 2) // 2))
    cmd = [find_blender(), "--background", "--factory-startup", "--threads", threads,
           "--python", os.path.join(HERE, "compass_scene.py"), "--", out, "1024", only]
    print("render:", " ".join(cmd))
    flags = getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0)
    subprocess.run(cmd, check=True, creationflags=flags)


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

    shots = []
    for h in (0, 45, 90, 215):       # camera headings; the objective sits at bearing 120 (ESE) throughout
        img = compose(h, 120 - h)
        img.save(os.path.join(out_dir, f"composite-heading{h:03d}-quest120.png"))
        shots.append(img)
    sheet = Image.new("RGBA", (SIZE * 2, SIZE * 2 + 200), (24, 26, 30, 255))
    for i, img in enumerate(shots):
        sheet.alpha_composite(img, ((i % 2) * SIZE, (i // 2) * SIZE))
        small = img.resize((180, 180), Image.LANCZOS)   # about the size it takes on a 1440p HUD
        sheet.alpha_composite(small, (20 + i * 250, SIZE * 2 + 10))
    sheet.save(os.path.join(out_dir, "sheet.png"))
    print(f"previews: {out_dir}")


def check():
    import numpy as np
    def arr(img):
        return np.asarray(img.convert("RGBA")).astype(np.float32)
    rose_img = Image.open(os.path.join(HERE, "layers", "rose.png"))
    needle_img = Image.open(os.path.join(HERE, "layers", "needle.png"))
    rose = arr(rose_img)
    S = rose.shape[0]
    c = (S - 1) / 2
    yy, xx = np.mgrid[0:S, 0:S]
    r = np.hypot(xx - c, yy - c)
    ang = (np.degrees(np.arctan2(xx - c, -(yy - c))) + 360) % 360   # 0 = up, clockwise
    R, G, B, A = rose[..., 0], rose[..., 1], rose[..., 2], rose[..., 3]
    gold = (A > 128) & (R > 70) & (R > B * 1.5) & (G > B * 1.1)
    # the braided cord: gold pixels in the knot's radial band (0.36..0.497 of the half-width; the bead ring is at 0.505)
    half = S / 2
    band = gold & (r > 0.36 * half) & (r < 0.497 * half)
    outer = np.zeros(360)
    for d in range(360):
        m = band & (np.abs(((ang - d + 180) % 360) - 180) < 0.5)
        outer[d] = r[m].max() if m.any() else 0.0
    cord_min = outer[outer > 0].min()
    print(f"cord outer edge: narrowest {cord_min:.1f} px at {int(np.argmin(np.where(outer > 0, outer, 1e9)))} deg, "
          f"widest {outer.max():.1f} px (of {half:.0f} px half-width)")
    # the gilt N on the rose, north up: gold pixels at radius >= 0.55 within 10 degrees of north
    n_mask = gold & (r > 0.55 * half) & (r < 0.72 * half) & ((ang < 10) | (ang > 350))
    nrgb = rose[n_mask][:, :3].mean(0)
    print(f"gilt N: {int(n_mask.sum())} px, radius {r[n_mask].min():.1f}-{r[n_mask].max():.1f} px, mean RGB {nrgb.round(0)}")
    ok = True
    for deg in (0, 45, 90):
        nd = arr(needle_img.rotate(-deg, resample=Image.BICUBIC))
        m = nd[..., 3] > 8
        rmax = r[m].max()
        verdict = "inside" if rmax < cord_min else "OUTSIDE"
        ok &= rmax < cord_min
        print(f"needle at {deg:3d} deg: max radius {rmax:.1f} px ({rmax / half:.3f}) vs cord {cord_min:.1f} px -> {verdict}")
    worst = 0
    for deg in range(360):
        nd = arr(needle_img.rotate(-deg, resample=Image.BICUBIC))
        worst = max(worst, int((n_mask & (nd[..., 3] > 8)).sum()))
    print(f"gilt N pixels under the needle, worst angle of 360: {worst}")
    ok &= worst == 0
    print("CHECK", "PASS" if ok else "FAIL")
    return ok


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--only", default="")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--previews")
    a = ap.parse_args()
    if a.render:
        render(a.only)
    reduce_layers()
    build_swf()
    if a.previews:
        previews(a.previews)
    if a.check and not check():
        sys.exit(1)
