# Blender 4.5 Cycles renders of the stop-display replica (the MakerWorld images).
# Run through make_renders.ps1, or directly:
#   blender -b -P render.py -- <build_dir> <screen_png> <out_dir> <samples> [shot ...]
# <build_dir> holds stl/ (from export.scad) and params.json. The background is a CC0
# photo of Vienna trams (Wikimedia Commons), blurred, lit by a CC0 urban HDRI from
# Poly Haven (zawiszy_czarnego); make_renders.ps1 downloads both into assets/.
import bpy, bmesh, json, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
BUILD, SCREEN, OUT, SAMPLES = argv[0], argv[1], argv[2], int(argv[3])
STL = os.path.join(BUILD, "stl")
SHOTS = argv[4:] or ["hero", "display", "back", "detail", "night", "exploded", "exploded_night", "static", "static_back"]
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets")
HDRI = os.path.join(ASSETS, "zawiszy_czarnego_2k.hdr")
PLATE = os.path.join(ASSETS, "vienna_tram_stop.jpg")
MM = 0.001
# from export.scad: base bottom, window centre x, active area w/h, glass front (mm)
_p = json.load(open(os.path.join(BUILD, "params.json")))
BASE_Z = _p[0] * MM
WIN_CX, AA_W, AA_H, GLASS_Y = _p[1] * MM, _p[2] * MM, _p[3] * MM, _p[4] * MM

def srgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(((x + 0.055) / 1.055) ** 2.4 if x > 0.04045 else x / 12.92 for x in c) + (1.0,)

def material(name, color, rough=0.45, metal=0.0, coat=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = srgb(color)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    b.inputs["Coat Weight"].default_value = coat
    return m

def materials():
    return {
        "box": material("box grey (RAL 7035)", "#c4c6c2", 0.5),
        "grey": material("grey PLA", "#a7a9a8", 0.5),
        "dark": material("dark grey PLA", "#55585a", 0.5),
        "black": material("black PLA", "#121212", 0.4),
        "red": material("red PLA", "#c8161d", 0.35, coat=0.3),
        "orange": material("orange PLA", "#f26a10", 0.45),
        "concrete": material("concrete grey PLA", "#b9b8b2", 0.6),
        "steel": material("galvanised", "#b4b8bb", 0.35, metal=0.6),
        "pcb": material("PCB", "#1d4f2e", 0.35, coat=0.6),
        "matte": material("black PLA, textured plate", "#0d0d0d", 0.6),
        "metal": material("metal", "#cfcfcf", 0.25, metal=1.0),
    }

BOARD = {"board_lcd": "black", "board_pcb": "pcb", "board_metal": "metal", "board_black": "black"}

# Exploded view: every part moved along the way it goes on (mm)
EXPLODE = {
    "board_lcd": (0, 24, 0), "board_pcb": (0, 24, 0), "board_metal": (0, 24, 0), "board_black": (0, 24, 0),
    "back": (0, 50, 0), "back_labels": (0, 50, 0),
    "pole": (0, 0, -24), "pole_cover": (0, 24, -24),
    "pole_top": (0, 0, 10), "cap": (0, 0, 26),
    "speaker_bracket": (24, 0, 20), "horns": (24, 0, 8),
    "base": (0, 0, -52), "base_strip": (0, 0, -40),
    "fence": (40, 0, -30), "can_grey": (16, 0, -30), "can_orange": (16, 0, -30),
}

PARTS = {
    "shell": "box", "labels": "black", "back": "box", "back_labels": "black",
    "pole": "grey", "pole_top": "grey", "pole_cover": "grey", "speaker_bracket": "dark",
    "horns": "grey", "cap": "red", "base": "concrete", "base_strip": "concrete",
    "fence": "steel", "can_grey": "grey", "can_orange": "orange",
}

# Static model: the display stand-in instead of the board, the back without pin slits
STATIC = {k: v for k, v in PARTS.items() if k not in ("back", "back_labels")}
STATIC.update({"back_closed": "box", "back_closed_labels": "black", "screen_dummy": "matte"})

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("OPTIX", "CUDA", "HIP", "ONEAPI", "METAL"):
        try:
            prefs.compute_device_type = kind
            prefs.get_devices()
            if any(d.type == kind for d in prefs.devices):
                for d in prefs.devices:
                    d.use = True
                sc.cycles.device = "GPU"
                print("render device:", kind)
                break
        except Exception:
            pass
    sc.cycles.samples = SAMPLES
    sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1440
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.look = "AgX - Medium High Contrast"
    return sc

def import_parts(offsets=None, board=False, parts=None):
    MAT = materials()
    parts = parts or (dict(PARTS, **BOARD) if board else PARTS)
    for name, mat in parts.items():
        bpy.ops.wm.stl_import(filepath=os.path.join(STL, name + ".stl"), global_scale=MM)
        ob = bpy.context.selected_objects[0]
        ob.name = name
        if offsets:
            ob.location = Vector(offsets.get(name, (0, 0, 0))) * MM
        ob.data.materials.append(MAT[mat])
        try:
            bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35))
        except Exception:
            bpy.ops.object.shade_auto_smooth(angle=math.radians(35))

def screen(strength, dy=0.0):
    # black cover glass behind the window, and the lit active area showing the real UI frame
    y = GLASS_Y + dy * MM
    bpy.ops.mesh.primitive_plane_add(size=1, location=(WIN_CX, y - 0.02 * MM, 0), rotation=(math.radians(90), 0, 0))
    g = bpy.context.object
    g.scale = (AA_W + 3 * MM, AA_H + 3 * MM, 1)
    g.data.materials.append(material("cover glass", "#030303", 0.3, coat=0.15))
    bpy.ops.mesh.primitive_plane_add(size=1, location=(WIN_CX, y - 0.04 * MM, 0), rotation=(math.radians(90), 0, 0))
    s = bpy.context.object
    s.scale = (AA_W, AA_H, 1)
    m = bpy.data.materials.new("screen")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0, 0, 0, 1)
    b.inputs["Roughness"].default_value = 0.3
    b.inputs["Specular IOR Level"].default_value = 0.2
    b.inputs["Coat Weight"].default_value = 0.15
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(SCREEN)
    tex.interpolation = "Closest"
    nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"])
    b.inputs["Emission Strength"].default_value = strength
    s.data.materials.append(m)   # the plane faces -y; its u runs along +x, so the frame reads left to right from the front

def backdrop(az, color, emission=0.0, floor=BASE_Z):
    # cyclorama: floor curving up into a wall behind the model, turned to face the camera
    bm = bmesh.new()
    prof = [(-1.2, 0.0), (0.12, 0.0)]
    r = 0.3
    for i in range(1, 13):
        a = math.radians(90 * i / 12)
        prof.append((0.12 + r * math.sin(a), r * (1 - math.cos(a))))
    prof.append((0.12 + r, 1.5))
    rows = []
    for x in (-1.5, 1.5):
        rows.append([bm.verts.new((x, y, z)) for y, z in prof])
    for i in range(len(prof) - 1):
        bm.faces.new((rows[0][i], rows[1][i], rows[1][i + 1], rows[0][i + 1]))
    me = bpy.data.meshes.new("backdrop")
    bm.to_mesh(me)
    ob = bpy.data.objects.new("backdrop", me)
    bpy.context.collection.objects.link(ob)
    ob.location = (0, 0, floor)
    ob.rotation_euler = (0, 0, math.radians(az))
    for p in me.polygons:
        p.use_smooth = True
    m = material("backdrop", color, 0.9)
    if emission:
        b = m.node_tree.nodes["Principled BSDF"]
        b.inputs["Emission Color"].default_value = srgb(color)
        b.inputs["Emission Strength"].default_value = emission
    me.materials.append(m)

def world(color, strength):
    w = bpy.data.worlds.new("world")
    bpy.context.scene.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = srgb(color)
    bg.inputs["Strength"].default_value = strength

def area(name, loc, target, power, size, color="#ffffff"):
    d = bpy.data.lights.new(name, "AREA")
    d.energy = power
    d.size = size
    d.color = srgb(color)[:3]
    ob = bpy.data.objects.new(name, d)
    bpy.context.collection.objects.link(ob)
    ob.location = loc
    aim(ob, target)
    return ob

def aim(ob, target):
    ob.rotation_euler = (Vector(target) - ob.location).to_track_quat("-Z", "Y").to_euler()

def camera(target, az, el, dist, lens, fstop=None, focus=None):
    c = bpy.data.cameras.new("cam")
    c.lens = lens
    if fstop:
        c.dof.use_dof = True
        c.dof.aperture_fstop = fstop
        c.dof.focus_distance = focus if focus else dist
    ob = bpy.data.objects.new("cam", c)
    bpy.context.collection.objects.link(ob)
    t = Vector(target)
    a, e = math.radians(az), math.radians(el)
    ob.location = t + dist * Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))
    aim(ob, t)
    bpy.context.scene.camera = ob
    return ob

def sun_rotation(img, az):
    # Turn the HDRI about z so its brightest spot (the sun) lies at azimuth az - 50,
    # i.e. front left of a camera at azimuth az. Blender's equirect lookup: u = atan2(y, -x) / 2pi + 0.5.
    import numpy as np
    wdt, hgt = img.size
    px = np.empty(wdt * hgt * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    lum = px.reshape(hgt, wdt, 4)[:, :, :3].sum(axis=2)
    row, col = np.unravel_index(int(np.argmax(lum)), lum.shape)
    alpha = math.degrees((col + 0.5) / wdt * 2 * math.pi - math.pi)
    print("hdri sun at u=%.3f v=%.3f" % ((col + 0.5) / wdt, (row + 0.5) / hgt))
    return (180 - alpha) - ((az - 50) - 90)

def vienna(cam, target, hdri_rot=0.0, blur=24, strength=0.7, az=None):
    # The model is rendered on a transparent background, lit by the HDRI, with its
    # shadow from a shadow-catcher floor in the alpha. The compositor lays that over
    # the Vienna photo, blurred, so the photo keeps its own colours.
    sc = bpy.context.scene
    sc.render.film_transparent = True
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    w = bpy.data.worlds.new("world")
    sc.world = w
    w.use_nodes = True
    nt = w.node_tree
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mapping = nt.nodes.new("ShaderNodeMapping")
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(HDRI)
    if az is not None:
        hdri_rot = sun_rotation(env.image, az)
    mapping.inputs["Rotation"].default_value = (0, 0, math.radians(hdri_rot))
    nt.links.new(tc.outputs["Generated"], mapping.inputs["Vector"])
    nt.links.new(mapping.outputs["Vector"], env.inputs["Vector"])
    nt.links.new(env.outputs["Color"], nt.nodes["Background"].inputs["Color"])
    nt.nodes["Background"].inputs["Strength"].default_value = strength
    bpy.ops.mesh.primitive_plane_add(size=3, location=(0, 0, BASE_Z))
    bpy.context.object.is_shadow_catcher = True
    bpy.context.view_layer.cycles.use_pass_shadow_catcher = True
    sc.use_nodes = True
    ct = sc.node_tree
    for n in list(ct.nodes):
        ct.nodes.remove(n)
    rl = ct.nodes.new("CompositorNodeRLayers")
    photo = ct.nodes.new("CompositorNodeImage")
    photo.image = bpy.data.images.load(PLATE)
    scale = ct.nodes.new("CompositorNodeScale")
    scale.space = "RENDER_SIZE"
    scale.frame_method = "CROP"
    gb = ct.nodes.new("CompositorNodeBlur")
    gb.filter_type = "GAUSS"
    gb.size_x = gb.size_y = blur
    over = ct.nodes.new("CompositorNodeAlphaOver")
    comp = ct.nodes.new("CompositorNodeComposite")
    shade = ct.nodes.new("CompositorNodeMixRGB")   # darken the photo where the model shades the floor
    shade.blend_type = "MULTIPLY"
    shade.inputs["Fac"].default_value = 1.0
    ct.links.new(photo.outputs["Image"], scale.inputs["Image"])
    ct.links.new(scale.outputs["Image"], gb.inputs["Image"])
    ct.links.new(gb.outputs["Image"], shade.inputs[1])
    ct.links.new(rl.outputs["Shadow Catcher"], shade.inputs[2])
    ct.links.new(shade.outputs["Image"], over.inputs[1])
    ct.links.new(rl.outputs["Image"], over.inputs[2])
    ct.links.new(over.outputs["Image"], comp.inputs["Image"])

def studio(az, backdrop_color="#6e7a88", floor=BASE_Z):
    world("#dfe4ea", 0.08)
    backdrop(az, backdrop_color, floor=floor)
    k = math.radians(az)
    def around(deg, dist, h):
        a = k + math.radians(deg)
        return (dist * math.sin(a), -dist * math.cos(a), h)
    area("key", around(-40, 0.55, 0.35), (0, 0, -0.05), 13, 0.45, "#fff4e8")
    area("fill", around(55, 0.6, 0.1), (0, 0, -0.06), 4, 0.6, "#e8f0ff")
    area("rim", around(160, 0.5, 0.35), (0, 0, -0.02), 10, 0.3)

def outdoor(cam, target, az, hdri_rot=0.0):
    # Vienna photo behind, HDRI daylight, plus a soft key from the camera's left for shape
    vienna(cam, target, az=az, strength=0.5)
    # key ("sun") from the camera's front left and above: casts the shadow onto the photo
    k = math.radians(az - 50)
    area("key", (0.6 * math.sin(k), -0.6 * math.cos(k), 0.55), (0, 0, -0.07), 9, 0.25, "#fff1dc")

def render(name):
    path = os.path.join(OUT, name + ".png")
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("wrote", path)

for shot in SHOTS:
    reset()
    if shot.startswith("exploded"):
        import_parts(EXPLODE, board=True)
    elif shot.startswith("static"):
        import_parts(parts=STATIC)
    else:
        import_parts()
    if shot == "exploded_night":
        # exploded layout in the night look: dark backdrop, warm lamp front left, cool sky behind
        floor = BASE_Z - 52 * MM
        screen(5.0, dy=24)
        world("#0b1424", 0.25)
        backdrop(132, "#1a2232", floor=floor)
        def around(deg, dist, h):
            a = math.radians(132 + deg)
            return (dist * math.sin(a), -dist * math.cos(a), h)
        area("street lamp", around(-45, 0.45, 0.45), (0, 0, -0.1), 30, 0.15, "#ffc98a")
        area("sky", around(150, 0.5, 0.3), (0, 0, -0.08), 14, 0.6, "#7f9cff")
        camera((0.016, 0.014, -0.074), 132, 16, 0.72, 70, fstop=11)
        render(shot)
        continue
    if shot == "exploded":
        # from behind on the right: board between shell and back cover, base and railing pulled apart
        floor = BASE_Z - 52 * MM
        screen(2.5, dy=24)
        studio(140, floor=floor)
        camera((0.016, 0.014, -0.074), 132, 16, 0.72, 70, fstop=11)
    elif shot == "hero":
        screen(2.5)
        t = (0.006, 0.0, -0.056)
        outdoor(camera(t, 25, 12, 0.52, 70, fstop=8), t, 25)
    elif shot == "display":
        screen(2.5)
        t = (WIN_CX + 0.006, 0.0, -0.004)
        outdoor(camera(t, -18, 6, 0.2, 100, fstop=16, focus=0.2), t, -18)
    elif shot == "back":
        screen(2.5)
        t = (0.0, 0.0, -0.058)
        outdoor(camera(t, 150, 14, 0.5, 70, fstop=8), t, 150)
    elif shot == "night":
        screen(5.0)
        world("#0b1424", 0.25)
        backdrop(25, "#1a2232")
        area("street lamp", (-0.25, -0.35, 0.45), (0, 0, -0.08), 22, 0.15, "#ffc98a")
        area("sky", (0.35, 0.2, 0.3), (0, 0, -0.05), 10, 0.6, "#7f9cff")
        camera((0.0, 0.0, -0.058), 25, 12, 0.56, 70, fstop=8)
    elif shot == "static":
        # the model without electronics: switched-off display, same view as the hero
        t = (0.006, 0.0, -0.056)
        outdoor(camera(t, 25, 12, 0.52, 70, fstop=8), t, 25)
    elif shot == "static_back":
        # ... and from behind: the same face as the front
        t = (0.0, 0.0, -0.058)
        outdoor(camera(t, 150, 14, 0.5, 70, fstop=8), t, 150)
    elif shot == "detail":
        # the bin faces the platform (-x): look at it from the front left, track behind
        screen(2.5)
        t = (0.016, -0.012, -0.092)
        outdoor(camera(t, -55, 16, 0.24, 70, fstop=11), t, -55)
    render(shot)
