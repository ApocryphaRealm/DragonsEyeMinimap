r"""Dragon's Eye Minimap - the Nordic compass, built and rendered in Blender (DEM 1.9.0).

Run (Blender 5.x, no GUI):
    blender --background --factory-startup --python tools/compass/compass_scene.py -- <out_dir> [size]

Everything is procedural: no image, font or model comes from outside this file. It renders four
transparent PNG layers, each seen straight down through an orthographic camera that covers -1..1 in X and Y
(the compass's outer edge is radius 0.98, its centre the image centre):

    body.png    the fixed bowl: stone rim with carved Elder Futhark runes, an iron bezel with a lubber
                wedge at the top, a dark slate face with engraved circles and bronze-inlaid ticks
    rose.png    the compass card that TURNS with the camera heading: a faceted eight-point rose in
                aged bronze, an interlaced (over-under) knotwork band, N / E / S / W in angular,
                rune-cut letterforms - north up in the image
    needle.png  the quest needle that turns to the objective's bearing: a red-enamelled spear arm on
                an iron spine, a counterweight tail - pointing up in the image
    cap.png     fixed, on top: the iron boss over the pivot and a faint glass sheen

The rose, needle and cap each throw their shadow onto a shadow catcher at the face's height, so the
shadow is in that layer's alpha and moves with it. The DLL stacks the four in this order and sets the
rose's and the needle's _rotation; tools/compass/build_compass.py turns these PNGs into the SWF.
"""
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else "."
SIZE = int(argv[1]) if len(argv) > 1 else 1024

R_OUT = 0.98      # stone rim outer edge
R_RIM_IN = 0.80   # stone rim inner edge
R_FACE = 0.785    # slate face

# ------------------------------------------------------------------------------------------------ scene
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 128
scene.cycles.use_denoising = True
scene.render.film_transparent = True
scene.render.resolution_x = SIZE
scene.render.resolution_y = SIZE
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.image_settings.color_depth = "8"
scene.view_settings.view_transform = "Standard"
scene.view_settings.look = "None"

world = bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes["Background"]
bg.inputs["Color"].default_value = (0.16, 0.175, 0.2, 1.0)    # cold, overcast
bg.inputs["Strength"].default_value = 0.55

cam_data = bpy.data.cameras.new("Cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = 2.0
cam = bpy.data.objects.new("Cam", cam_data)
cam.location = (0.0, 0.0, 6.0)
scene.collection.objects.link(cam)
scene.camera = cam

def add_light(name, kind, loc, aim, energy, color, size=None):
    data = bpy.data.lights.new(name, kind)
    data.energy = energy
    data.color = color
    if size is not None:
        if kind == "AREA":
            data.size = size
        elif kind == "SUN":
            data.angle = size
    obj = bpy.data.objects.new(name, data)
    obj.location = loc
    obj.rotation_euler = (Vector(aim) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(obj)
    return obj

# key light from the upper left, cold; a weak warm fill from the lower right (a torch somewhere)
add_light("Key", "SUN", (-3.0, 3.0, 5.0), (0, 0, 0), 3.2, (0.86, 0.92, 1.0), size=math.radians(6))
add_light("Fill", "AREA", (3.0, -3.0, 3.5), (0, 0, 0), 60.0, (1.0, 0.78, 0.55), size=3.0)

collections = {}
def coll(name):
    if name not in collections:
        c = bpy.data.collections.new(name)
        scene.collection.children.link(c)
        collections[name] = c
    return collections[name]

def link(obj, layer):
    coll(layer).objects.link(obj)
    return obj

# ------------------------------------------------------------------------------------------------ helpers
def new_obj(name, layer, bm_or_mesh):
    if isinstance(bm_or_mesh, bmesh.types.BMesh):
        me = bpy.data.meshes.new(name)
        bm_or_mesh.to_mesh(me)
        bm_or_mesh.free()
    else:
        me = bm_or_mesh
    obj = bpy.data.objects.new(name, me)
    return link(obj, layer)

def shade_smooth(obj):
    for p in obj.data.polygons:
        p.use_smooth = True

def revolve(name, layer, profile, segs=192):
    """A ring from a (r, z) profile turned round the Z axis."""
    bm = bmesh.new()
    rings = []
    for i in range(segs):
        a = 2 * math.pi * i / segs
        ca, sa = math.cos(a), math.sin(a)
        rings.append([bm.verts.new((r * ca, r * sa, z)) for r, z in profile])
    for i in range(segs):
        r0, r1 = rings[i], rings[(i + 1) % segs]
        for j in range(len(profile) - 1):
            bm.faces.new((r0[j], r0[j + 1], r1[j + 1], r1[j]))
    obj = new_obj(name, layer, bm)
    shade_smooth(obj)
    return obj

def disc(name, layer, r, z, segs=192):
    bm = bmesh.new()
    c = bm.verts.new((0, 0, z))
    ring = [bm.verts.new((r * math.cos(2 * math.pi * i / segs), r * math.sin(2 * math.pi * i / segs), z)) for i in range(segs)]
    for i in range(segs):
        bm.faces.new((c, ring[i], ring[(i + 1) % segs]))
    return new_obj(name, layer, bm)

def strokes_mesh(name, layer, segments, width, z0, z1, bevel=0.0):
    """Boxes along line segments [((x0,y0),(x1,y1)), ...], width across, from z0 to z1."""
    bm = bmesh.new()
    for (x0, y0), (x1, y1) in segments:
        dx, dy = x1 - x0, y1 - y0
        ln = math.hypot(dx, dy) or 1.0
        ux, uy = dx / ln, dy / ln
        px, py = -uy * width / 2, ux * width / 2
        # extend each end by half the width so joints close
        ex, ey = ux * width / 2, uy * width / 2
        quad = [(x0 - ex + px, y0 - ey + py), (x1 + ex + px, y1 + ey + py), (x1 + ex - px, y1 + ey - py), (x0 - ex - px, y0 - ey - py)]
        bottom = [bm.verts.new((x, y, z0)) for x, y in quad]
        top = [bm.verts.new((x, y, z1)) for x, y in quad]
        bm.faces.new(top)
        bm.faces.new(list(reversed(bottom)))
        for i in range(4):
            j = (i + 1) % 4
            bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    obj = new_obj(name, layer, bm)
    if bevel > 0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        mod.limit_method = "ANGLE"
    return obj

def polyline(points):
    return [(points[i], points[i + 1]) for i in range(len(points) - 1)]

def place_glyph(segments, cx, cy, h, angle, w_box):
    """Glyph strokes in a unit box (x 0..w_box, y 0..1) -> world, centred at (cx, cy), height h, the glyph's up
    pointing outward along `angle` (radians, 0 = +Y, clockwise)."""
    out = []
    ca, sa = math.cos(-angle), math.sin(-angle)
    for (x0, y0), (x1, y1) in segments:
        pts = []
        for x, y in ((x0, y0), (x1, y1)):
            lx, ly = (x - w_box / 2) * h, (y - 0.5) * h
            pts.append((cx + lx * ca - ly * sa, cy + lx * sa + ly * ca))
        out.append((pts[0], pts[1]))
    return out

def polar(r, angle):
    """angle 0 = up (+Y), clockwise - the compass convention."""
    return r * math.sin(angle), r * math.cos(angle)

# ------------------------------------------------------------------------------------------------ materials
def principled(name, base, metallic=0.0, rough=0.5, coat=0.0, bump_img=None, bump_strength=0.0, noise_bump=0.0,
               noise_scale=40.0, color_var=0.0, inlay=None):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    p = nt.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*base, 1.0)
    p.inputs["Metallic"].default_value = metallic
    p.inputs["Roughness"].default_value = rough
    if coat > 0:
        p.inputs["Coat Weight"].default_value = coat
        p.inputs["Coat Roughness"].default_value = 0.08
    links = nt.links
    texco = nt.nodes.new("ShaderNodeTexCoord")
    height_sum = None
    if noise_bump > 0 or color_var > 0:
        noise = nt.nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = noise_scale
        noise.inputs["Detail"].default_value = 8.0
        noise.inputs["Roughness"].default_value = 0.62
        links.new(texco.outputs["Object"], noise.inputs["Vector"])
        if color_var > 0:
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.inputs["Factor"].default_value = color_var
            mix.inputs[6].default_value = (*base, 1.0)
            links.new(noise.outputs["Fac"], mix.inputs[7])
            mul = nt.nodes.new("ShaderNodeMix")
            mul.data_type = "RGBA"
            mul.blend_type = "MULTIPLY"
            mul.inputs["Factor"].default_value = 1.0
            mul.inputs[6].default_value = (*base, 1.0)
            links.new(mix.outputs[2], mul.inputs[7])
            # keep the mean brightness: noise ~0.5 -> x2
            bright = nt.nodes.new("ShaderNodeMix")
            bright.data_type = "RGBA"
            bright.blend_type = "MULTIPLY"
            bright.inputs["Factor"].default_value = 1.0
            links.new(mul.outputs[2], bright.inputs[6])
            bright.inputs[7].default_value = (2.0, 2.0, 2.0, 1.0)
            base_socket = bright.outputs[2]
            links.new(base_socket, p.inputs["Base Color"])
        if noise_bump > 0:
            height_sum = (noise.outputs["Fac"], noise_bump)
    mask_out = None
    if bump_img is not None:
        mapping = nt.nodes.new("ShaderNodeMapping")
        mapping.inputs["Location"].default_value = (0.5, 0.5, 0.0)
        mapping.inputs["Scale"].default_value = (0.5, 0.5, 1.0)
        links.new(texco.outputs["Object"], mapping.inputs["Vector"])
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = bump_img
        tex.interpolation = "Cubic"
        tex.extension = "CLIP"
        links.new(mapping.outputs["Vector"], tex.inputs["Vector"])
        mask_out = tex.outputs["Color"]
    if height_sum is not None or mask_out is not None:
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Distance"].default_value = 0.02
        if height_sum is not None and mask_out is not None:
            a = nt.nodes.new("ShaderNodeMath"); a.operation = "MULTIPLY"
            links.new(height_sum[0], a.inputs[0]); a.inputs[1].default_value = height_sum[1]
            b = nt.nodes.new("ShaderNodeMath"); b.operation = "MULTIPLY"
            sep = nt.nodes.new("ShaderNodeSeparateColor")
            links.new(mask_out, sep.inputs["Color"])
            links.new(sep.outputs["Red"], b.inputs[0]); b.inputs[1].default_value = -bump_strength
            s = nt.nodes.new("ShaderNodeMath"); s.operation = "ADD"
            links.new(a.outputs[0], s.inputs[0]); links.new(b.outputs[0], s.inputs[1])
            links.new(s.outputs[0], bump.inputs["Height"])
            # darken the grooves: grime in the cut
            dark = nt.nodes.new("ShaderNodeMix")
            dark.data_type = "RGBA"
            dark.blend_type = "MULTIPLY"
            links.new(sep.outputs["Red"], dark.inputs["Factor"])
            src = p.inputs["Base Color"].links[0].from_socket if p.inputs["Base Color"].is_linked else None
            if src is not None:
                links.new(src, dark.inputs[6])
            else:
                dark.inputs[6].default_value = (*base, 1.0)
            dark.inputs[7].default_value = (0.25, 0.25, 0.27, 1.0)
            links.new(dark.outputs[2], p.inputs["Base Color"])
            if inlay is not None:
                # second channel (green) of the mask: bronze inlay in the cut
                inl = nt.nodes.new("ShaderNodeMix")
                inl.data_type = "RGBA"
                links.new(sep.outputs["Green"], inl.inputs["Factor"])
                links.new(dark.outputs[2], inl.inputs[6])
                inl.inputs[7].default_value = (*inlay, 1.0)
                links.new(inl.outputs[2], p.inputs["Base Color"])
                met = nt.nodes.new("ShaderNodeMath"); met.operation = "MAXIMUM"
                met.inputs[0].default_value = metallic
                links.new(sep.outputs["Green"], met.inputs[1])
                links.new(met.outputs[0], p.inputs["Metallic"])
        elif height_sum is not None:
            a = nt.nodes.new("ShaderNodeMath"); a.operation = "MULTIPLY"
            links.new(height_sum[0], a.inputs[0]); a.inputs[1].default_value = height_sum[1]
            links.new(a.outputs[0], bump.inputs["Height"])
        else:
            sep = nt.nodes.new("ShaderNodeSeparateColor")
            links.new(mask_out, sep.inputs["Color"])
            b = nt.nodes.new("ShaderNodeMath"); b.operation = "MULTIPLY"
            links.new(sep.outputs["Red"], b.inputs[0]); b.inputs[1].default_value = -bump_strength
            links.new(b.outputs[0], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], p.inputs["Normal"])
    return mat

def assign(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    return obj

# ------------------------------------------------------------------------------------------------ runes
# Elder Futhark, drawn as straight strokes in a 0.6 x 1 box - the letters were made to be cut with a knife.
RUNES = {
    "fehu":     [((0, 0), (0, 1)), ((0, 0.55), (0.5, 0.85)), ((0, 0.8), (0.45, 1.0))],
    "uruz":     polyline([(0, 0), (0, 1), (0.5, 0.72), (0.5, 0)]),
    "thurisaz": [((0, 0), (0, 1))] + polyline([(0, 0.72), (0.42, 0.5), (0, 0.28)]),
    "ansuz":    [((0, 0), (0, 1)), ((0, 1), (0.42, 0.78)), ((0, 0.74), (0.42, 0.52))],
    "raidho":   [((0, 0), (0, 1))] + polyline([(0, 1), (0.42, 0.8), (0, 0.56), (0.46, 0)]),
    "kaunan":   polyline([(0.42, 1), (0, 0.5), (0.42, 0)]),
    "gebo":     [((0, 0), (0.6, 1)), ((0, 1), (0.6, 0))],
    "wunjo":    [((0, 0), (0, 1))] + polyline([(0, 1), (0.42, 0.8), (0, 0.6)]),
    "hagalaz":  [((0, 0), (0, 1)), ((0.5, 0), (0.5, 1)), ((0, 0.66), (0.5, 0.34))],
    "naudiz":   [((0.3, 0), (0.3, 1)), ((0.02, 0.66), (0.58, 0.36))],
    "isaz":     [((0.3, 0), (0.3, 1))],
    "jera":     polyline([(0.32, 1), (0.02, 0.72), (0.32, 0.46)]) + polyline([(0.28, 0.54), (0.58, 0.28), (0.28, 0)]),
    "eihwaz":   [((0.3, 0), (0.3, 1)), ((0.3, 1), (0.56, 0.82)), ((0.3, 0), (0.04, 0.18))],
    "pertho":   [((0, 0), (0, 1))] + polyline([(0, 1), (0.42, 0.8), (0.42, 0.68)]) + polyline([(0, 0), (0.42, 0.2), (0.42, 0.32)]),
    "algiz":    [((0.3, 0), (0.3, 1)), ((0.3, 0.55), (0.0, 1.0)), ((0.3, 0.55), (0.6, 1.0))],
    "sowilo":   polyline([(0.48, 1), (0.1, 0.64), (0.5, 0.36), (0.12, 0)]),
    "tiwaz":    [((0.3, 0), (0.3, 1)), ((0.3, 1), (0.0, 0.7)), ((0.3, 1), (0.6, 0.7))],
    "berkano":  [((0, 0), (0, 1))] + polyline([(0, 1), (0.46, 0.75), (0, 0.5), (0.46, 0.25), (0, 0)]),
    "ehwaz":    [((0, 0), (0, 1)), ((0.6, 0), (0.6, 1))] + polyline([(0, 1), (0.3, 0.7), (0.6, 1)]),
    "mannaz":   [((0, 0), (0, 1)), ((0.6, 0), (0.6, 1)), ((0, 1), (0.6, 0.56)), ((0.6, 1), (0, 0.56))],
    "laguz":    [((0, 0), (0, 1)), ((0, 1), (0.46, 0.72))],
    "ingwaz":   polyline([(0.3, 1), (0.6, 0.5), (0.3, 0), (0, 0.5), (0.3, 1)]),
    "dagaz":    polyline([(0, 0), (0, 1), (0.6, 0), (0.6, 1), (0, 0)]),
    "othala":   polyline([(0.3, 1), (0.56, 0.72), (0.06, 0.32), (0.06, 0)]) + polyline([(0.3, 1), (0.04, 0.72), (0.54, 0.32), (0.54, 0)]),
}

def raster_mask(size, groove_segments, inlay_segments, groove_w, inlay_w):
    """An RGBA float image over -1..1: red = groove depth (V profile), green = bronze inlay."""
    xs = np.linspace(-1.0, 1.0, size, dtype=np.float32)
    X, Y = np.meshgrid(xs, xs)   # row 0 = y -1 (Blender images start at the bottom)
    def field(segments, w):
        out = np.zeros((size, size), dtype=np.float32)
        for (x0, y0), (x1, y1) in segments:
            # only the bounding box of the segment, for speed
            pad = w * 1.5
            c0 = max(0, int((min(x0, x1) - pad + 1) / 2 * size)); c1 = min(size, int((max(x0, x1) + pad + 1) / 2 * size) + 2)
            r0 = max(0, int((min(y0, y1) - pad + 1) / 2 * size)); r1 = min(size, int((max(y0, y1) + pad + 1) / 2 * size) + 2)
            if c0 >= c1 or r0 >= r1:
                continue
            px, py = X[r0:r1, c0:c1], Y[r0:r1, c0:c1]
            dx, dy = x1 - x0, y1 - y0
            l2 = dx * dx + dy * dy or 1e-9
            t = np.clip(((px - x0) * dx + (py - y0) * dy) / l2, 0.0, 1.0)
            d = np.hypot(px - (x0 + t * dx), py - (y0 + t * dy))
            v = np.clip(1.0 - d / (w / 2), 0.0, 1.0)
            out[r0:r1, c0:c1] = np.maximum(out[r0:r1, c0:c1], v)
        return out
    red = field(groove_segments, groove_w)
    green = np.clip(field(inlay_segments, inlay_w) * 3.0, 0.0, 1.0) if inlay_segments else np.zeros_like(red)
    rgba = np.stack([red, green, np.zeros_like(red), np.ones_like(red)], axis=-1)
    img = bpy.data.images.new("mask", size, size, alpha=True, float_buffer=True)
    img.colorspace_settings.name = "Non-Color"
    img.pixels.foreach_set(rgba.ravel())
    img.pack()
    return img

# ================================================================================================ BODY
rune_names = list(RUNES.keys())
rim_segments = []
R_RUNE = 0.892
RUNE_H = 0.088
for i, name in enumerate(rune_names):
    a = (i + 0.5) / len(rune_names) * 2 * math.pi
    cx, cy = polar(R_RUNE, a)
    rim_segments += place_glyph(RUNES[name], cx, cy, RUNE_H, a, 0.6)
# engraved circles on the face and a fine degree ring
face_segments = []
for rr in (0.30, 0.58, 0.705):
    n = 160
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        face_segments.append((polar(rr, a0), polar(rr, a1)))
inlay_segments = []
for i in range(64):
    a = 2 * math.pi * i / 64
    long = i % 8 == 0
    mid = i % 4 == 0
    r0 = 0.72 if long else (0.735 if mid else 0.75)
    inlay_segments.append((polar(r0, a), polar(0.775, a)))
mask = raster_mask(2048, rim_segments + face_segments + inlay_segments, inlay_segments, 0.014, 0.008)

stone = principled("Stone", (0.085, 0.09, 0.098), rough=0.88, bump_img=mask, bump_strength=0.55,
                   noise_bump=0.35, noise_scale=55.0, color_var=0.55)
slate = principled("Slate", (0.03, 0.033, 0.038), rough=0.62, bump_img=mask, bump_strength=0.35,
                   noise_bump=0.06, noise_scale=90.0, color_var=0.3, inlay=(0.42, 0.29, 0.15))
iron = principled("Iron", (0.16, 0.165, 0.175), metallic=1.0, rough=0.42, noise_bump=0.12, noise_scale=120.0, color_var=0.35)
bronze = principled("Bronze", (0.36, 0.255, 0.14), metallic=1.0, rough=0.4, noise_bump=0.05, noise_scale=160.0, color_var=0.4)
dark_bronze = principled("DarkBronze", (0.22, 0.16, 0.095), metallic=1.0, rough=0.46, noise_bump=0.06, noise_scale=140.0, color_var=0.45)
silver_iron = principled("ColdSteel", (0.55, 0.58, 0.62), metallic=1.0, rough=0.28, noise_bump=0.04, noise_scale=150.0, color_var=0.2)
enamel = principled("RedEnamel", (0.42, 0.025, 0.02), rough=0.3, coat=1.0, noise_bump=0.02, noise_scale=60.0, color_var=0.15)

rim = revolve("Rim", "body", [(R_RIM_IN, 0.045), (0.812, 0.072), (0.84, 0.084), (0.945, 0.084), (0.968, 0.07), (R_OUT, 0.035), (R_OUT, 0.0)])
assign(rim, stone)
face = disc("Face", "body", R_FACE, 0.0)
assign(face, slate)
bezel = revolve("Bezel", "body", [(0.772, 0.0), (0.776, 0.045), (0.79, 0.064), (0.806, 0.05), (0.81, 0.02), (0.81, 0.0)])
assign(bezel, iron)
# rivets in the bezel at the eight points
for i in range(8):
    a = (i + 0.5) / 8 * 2 * math.pi
    x, y = polar(0.791, a)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.011, location=(x, y, 0.062), segments=16, ring_count=8)
    rv = bpy.context.active_object
    for c in rv.users_collection:
        c.objects.unlink(rv)
    link(rv, "body"); assign(rv, iron); shade_smooth(rv)
# the lubber wedge: fixed, at the top, where the heading is read
bm = bmesh.new()
pts = [(0.0, 0.795, 0.11), (-0.03, 0.9, 0.095), (0.03, 0.9, 0.095)]
v = [bm.verts.new(p) for p in pts]
base = [bm.verts.new((p[0], p[1], 0.07)) for p in pts]
bm.faces.new(v)
for i in range(3):
    j = (i + 1) % 3
    bm.faces.new((base[i], base[j], v[j], v[i]))
lub = new_obj("Lubber", "body", bm)
assign(lub, silver_iron)
bev = lub.modifiers.new("Bevel", "BEVEL"); bev.width = 0.004; bev.segments = 2

# ================================================================================================ ROSE
def kite(name, angle, length, width, waist, z_c, z_t, mat_l, mat_r):
    """One rose point: two facets meeting on a raised ridge. A facet per material gives the light/dark halves."""
    objs = []
    for side, mat in ((-1, mat_l), (1, mat_r)):
        bm = bmesh.new()
        tip = Vector((*polar(length, angle), z_t))
        c = Vector((0.0, 0.0, z_c))
        sx, sy = polar(waist, angle)
        px, py = polar(width, angle + side * math.pi / 2)
        s = Vector((sx + px, sy + py, 0.02))
        verts = [bm.verts.new(c), bm.verts.new(tip), bm.verts.new(s)]
        if side < 0:
            bm.faces.new((verts[0], verts[2], verts[1]))
        else:
            bm.faces.new((verts[0], verts[1], verts[2]))
        o = new_obj(f"{name}_{side}", "rose", bm)
        assign(o, mat)
        objs.append(o)
    return objs

for i in range(8):
    a = i * math.pi / 4
    cardinal = i % 2 == 0
    if cardinal:
        north = i == 0
        kite(f"P{i}", a, 0.56, 0.085, 0.13, 0.085, 0.03, silver_iron if north else bronze, iron if north else dark_bronze)
    else:
        kite(f"P{i}", a, 0.36, 0.06, 0.09, 0.07, 0.03, dark_bronze, bronze)

# the knotwork: two bands braided round the card, crossing sixteen times, over and under in turn
def band(name, phase, R, amp, lobes, z0, dz, radius_bevel, mat):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius_bevel
    cu.bevel_resolution = 3
    cu.use_fill_caps = False
    sp = cu.splines.new("POLY")
    n = 720
    sp.points.add(n - 1)
    for k in range(n):
        t = 2 * math.pi * k / n
        r = R + amp * math.sin(lobes * t + phase)
        z = z0 + dz * math.cos(lobes * t + phase)
        x, y = polar(r, t)
        sp.points[k].co = (x, y, z, 1.0)
    sp.use_cyclic_u = True
    obj = bpy.data.objects.new(name, cu)
    link(obj, "rose")
    obj.data.materials.append(mat)
    obj.scale = (1.0, 1.0, 0.6)
    return obj

band("KnotA", 0.0, 0.43, 0.045, 8, 0.075, 0.018, 0.0135, bronze)
band("KnotB", math.pi, 0.43, 0.045, 8, 0.075, 0.018, 0.0135, bronze)
# an inner and outer bead ring round the card
for rr in (0.355, 0.505):
    cu = bpy.data.curves.new(f"Bead{rr}", "CURVE")
    cu.dimensions = "3D"; cu.bevel_depth = 0.006; cu.bevel_resolution = 2
    sp = cu.splines.new("POLY"); n = 360; sp.points.add(n - 1)
    for k in range(n):
        x, y = polar(rr, 2 * math.pi * k / n)
        sp.points[k].co = (x, y, 0.05, 1.0)
    sp.use_cyclic_u = True
    ob = bpy.data.objects.new(f"Bead{rr}", cu); link(ob, "rose"); ob.data.materials.append(dark_bronze)

# the hub plate under the pivot
hub = revolve("Hub", "rose", [(0.0, 0.1), (0.07, 0.098), (0.11, 0.085), (0.125, 0.07), (0.125, 0.05)], segs=96)
assign(hub, bronze)

# N E S W, angular letterforms cut like runes, raised in cold steel (N) and bronze, standing on the card
LETTERS = {
    "N": (0.62, [((0, 0), (0, 1)), ((0.62, 0), (0.62, 1)), ((0, 1), (0.62, 0)), ((0, 0), (-0.06, -0.04)), ((0.62, 1), (0.68, 1.04))]),
    "E": (0.5, [((0, 0), (0, 1)), ((0, 1), (0.5, 1.06)), ((0, 0.52), (0.36, 0.52)), ((0, 0.0), (0.5, -0.06))]),
    "S": (0.56, polyline([(0.56, 0.98), (0.04, 0.66), (0.54, 0.36), (0.02, 0.02)])),
    "W": (0.82, polyline([(0, 1), (0.2, 0), (0.41, 0.62), (0.62, 0), (0.82, 1)])),
}
R_LETTER = 0.632
LETTER_H = 0.105
for i, ch in enumerate("NESW"):
    a = i * math.pi / 2
    w_box, segs = LETTERS[ch]
    cx, cy = polar(R_LETTER, a)
    lw = 0.024 if ch == "N" else 0.019
    obj = strokes_mesh(f"L_{ch}", "rose", place_glyph(segs, cx, cy, LETTER_H, 0.0, w_box), lw, 0.03, 0.06, bevel=0.004)
    # letters stand upright on the card with north up: their base faces the centre on every side would be
    # the nautical style; upright reading (all upright when north is up) is the vanilla compass's. Keep them radial:
    obj.rotation_euler = (0, 0, 0)
    # rotate about the card centre so each letter's up points outward
    obj.select_set(False)
    assign(obj, silver_iron if ch == "N" else bronze)
# radial orientation: rebuild each letter rotated so its top faces outward (the card turns, so radial reads best)
for i, ch in enumerate("NESW"):
    obj = bpy.data.objects[f"L_{ch}"]
    me = obj.data
    a = i * math.pi / 2
    cx, cy = polar(R_LETTER, a)
    ca, sa = math.cos(-a), math.sin(-a)
    for vtx in me.vertices:
        x, y = vtx.co.x - cx, vtx.co.y - cy
        vtx.co.x = cx + x * ca - y * sa
        vtx.co.y = cy + x * sa + y * ca

# ================================================================================================ NEEDLE
def outline_poly(points, z, name, layer, mat, z_top=None, bevel=0.0):
    """A flat polygon (convex or star-shaped about its first point's centroid) as a slab."""
    bm = bmesh.new()
    zt = z_top if z_top is not None else z + 0.01
    bot = [bm.verts.new((x, y, z)) for x, y in points]
    top = [bm.verts.new((x, y, zt)) for x, y in points]
    bm.faces.new(top)
    bm.faces.new(list(reversed(bot)))
    n = len(points)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((bot[i], bot[j], top[j], top[i]))
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    o = new_obj(name, layer, bm)
    assign(o, mat)
    if bevel > 0:
        m = o.modifiers.new("Bevel", "BEVEL"); m.width = bevel; m.segments = 2; m.limit_method = "ANGLE"
    return o

def spear(scale_w, tip_y, blade_base, blade_w, shaft_w, tail_y):
    """The needle's outline, pointing up: a leaf-shaped spearhead, a shaft, a forked tail."""
    pts = []
    # right side, tip to tail
    pts.append((0.0, tip_y))
    for k in range(1, 9):
        t = k / 9
        y = tip_y - (tip_y - blade_base) * t
        w = blade_w * math.sin(math.pi * (0.15 + 0.85 * t) ** 0.9 * 0.82) * scale_w
        pts.append((w, y))
    pts.append((shaft_w * scale_w * 1.6, blade_base - 0.005))
    pts.append((shaft_w * scale_w, blade_base - 0.03))
    pts.append((shaft_w * scale_w, tail_y + 0.06))
    pts.append((shaft_w * scale_w * 3.2, tail_y - 0.005))
    pts.append((shaft_w * scale_w * 1.2, tail_y + 0.025))
    pts.append((0.0, tail_y + 0.01))
    left = [(-x, y) for x, y in reversed(pts[1:-1])]
    return pts + left

iron_outline = spear(1.0, 0.748, 0.53, 0.075, 0.021, -0.26)
outline_poly(iron_outline, 0.10, "NeedleIron", "needle", iron, z_top=0.118, bevel=0.003)
enamel_inner = spear(0.62, 0.715, 0.545, 0.075, 0.021, -0.225)
outline_poly(enamel_inner, 0.118, "NeedleEnamel", "needle", enamel, z_top=0.124)
# a cold-steel spine down the blade
strokes_mesh("NeedleSpine", "needle", [((0.0, 0.70), (0.0, 0.56))], 0.007, 0.124, 0.13, bevel=0.002)
assign(bpy.data.objects["NeedleSpine"], silver_iron)
# the pivot ring of the needle
pr = revolve("NeedleRing", "needle", [(0.03, 0.118), (0.045, 0.128), (0.06, 0.118), (0.06, 0.1)], segs=64)
assign(pr, iron)

# ================================================================================================ CAP
boss = revolve("Boss", "cap", [(0.0, 0.17), (0.02, 0.168), (0.035, 0.16), (0.045, 0.145), (0.05, 0.13), (0.05, 0.125)], segs=96)
assign(boss, iron)
# a tiny knot on the boss: four lobes
for i in range(4):
    a = i * math.pi / 2 + math.pi / 4
    x, y = polar(0.026, a)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.009, location=(x, y, 0.163), segments=16, ring_count=8)
    rv = bpy.context.active_object
    for c in rv.users_collection:
        c.objects.unlink(rv)
    link(rv, "cap"); assign(rv, dark_bronze); shade_smooth(rv)

# glass: a faint sheen across the face, brightest at the upper left, as partial alpha
glass_mat = bpy.data.materials.new("Glass")
glass_mat.use_nodes = True
nt = glass_mat.node_tree
for n_ in list(nt.nodes):
    nt.nodes.remove(n_)
out = nt.nodes.new("ShaderNodeOutputMaterial")
mixs = nt.nodes.new("ShaderNodeMixShader")
transp = nt.nodes.new("ShaderNodeBsdfTransparent")
emit = nt.nodes.new("ShaderNodeEmission")
emit.inputs["Color"].default_value = (0.85, 0.9, 1.0, 1.0)
emit.inputs["Strength"].default_value = 1.0
texco = nt.nodes.new("ShaderNodeTexCoord")
mapping = nt.nodes.new("ShaderNodeMapping")
mapping.inputs["Location"].default_value = (0.42, -0.42, 0.0)
mapping.inputs["Scale"].default_value = (1.25, 1.25, 1.0)
grad = nt.nodes.new("ShaderNodeTexGradient")
grad.gradient_type = "SPHERICAL"
ramp = nt.nodes.new("ShaderNodeValToRGB")
ramp.color_ramp.elements[0].position = 0.0
ramp.color_ramp.elements[0].color = (0, 0, 0, 1)
ramp.color_ramp.elements[1].position = 1.0
ramp.color_ramp.elements[1].color = (0.13, 0.13, 0.13, 1)
e = ramp.color_ramp.elements.new(0.55)
e.color = (0.035, 0.035, 0.035, 1)
nt.links.new(texco.outputs["Object"], mapping.inputs["Vector"])
nt.links.new(mapping.outputs["Vector"], grad.inputs["Vector"])
nt.links.new(grad.outputs["Fac"], ramp.inputs["Fac"])
nt.links.new(ramp.outputs["Color"], mixs.inputs["Fac"])
nt.links.new(transp.outputs[0], mixs.inputs[1])
nt.links.new(emit.outputs[0], mixs.inputs[2])
nt.links.new(mixs.outputs[0], out.inputs["Surface"])
glass = disc("Glass", "cap", 0.77, 0.2)
assign(glass, glass_mat)
glass.visible_shadow = False

# ------------------------------------------------------------------------------------------------ shadow catchers
catcher = disc("Catcher", "catcher", 0.97, 0.0, segs=96)
catcher.is_shadow_catcher = True

# ------------------------------------------------------------------------------------------------ render each layer
# each moving layer's shadow falls just under it, so a shadow that turns with its layer stays short
LAYERS = [("body", [], 0.0), ("rose", ["catcher"], 0.0), ("needle", ["catcher"], 0.075), ("cap", ["catcher"], 0.11)]
for layer, extra, catcher_z in LAYERS:
    catcher.location.z = catcher_z
    for name, c in collections.items():
        c.hide_render = not (name == layer or name in extra)
    scene.render.filepath = f"{OUT}/{layer}.png"
    bpy.ops.render.render(write_still=True)
    print(f"[compass] rendered {layer} -> {scene.render.filepath}")
