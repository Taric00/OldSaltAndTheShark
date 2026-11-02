"""Creates M_StylizedLake (unlit translucent, depth-fade foam) and assigns it to DenizMesh.

Run in the editor (Output Log -> Python, or Tools > Execute Python Script) or headless with
UnrealEditor-Cmd -ExecutePythonScript.
"""
import unreal

MAT_DIR = "/Game/Materials"
MAT_NAME = "M_StylizedLake"
MESH_PATH = "/Game/Models/DenizMesh"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

if not eal.does_directory_exist(MAT_DIR):
    eal.make_directory(MAT_DIR)

mat_path = f"{MAT_DIR}/{MAT_NAME}"
if eal.does_asset_exist(mat_path):
    mat = eal.load_asset(mat_path)
    mel.delete_all_material_expressions(mat)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

# Unlit + translucent: no lighting cost, scene depth is available for the intersection foam.
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x * 220, y * 160)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(src, dst, pin, src_out=""):
    # single-input nodes expose an unnamed pin, so fall back to ""
    if not (mel.connect_material_expressions(src, src_out, dst, pin)
            or (pin == "Input" and mel.connect_material_expressions(src, src_out, dst, ""))):
        raise RuntimeError(f"connect failed: {src.get_name()} -> {dst.get_name()}.{pin}")
    return dst


def scalar(name, value, x, y):
    return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def color(name, rgb, x, y):
    return node(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def mul(a, b, x, y):
    n = node(unreal.MaterialExpressionMultiply, x, y)
    link(a, n, "A")
    link(b, n, "B")
    return n


def add(a, b, x, y):
    n = node(unreal.MaterialExpressionAdd, x, y)
    link(a, n, "A")
    link(b, n, "B")
    return n


def sub(a, b, x, y):
    n = node(unreal.MaterialExpressionSubtract, x, y)
    link(a, n, "A")
    link(b, n, "B")
    return n


def lerp(a, b, alpha, x, y):
    n = node(unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, n, "A")
    link(b, n, "B")
    link(alpha, n, "Alpha")
    return n


def one_minus(a, x, y):
    return link(a, node(unreal.MaterialExpressionOneMinus, x, y), "Input")


def saturate(a, x, y):
    return link(a, node(unreal.MaterialExpressionSaturate, x, y), "Input")


def sine(a, x, y):
    return link(a, node(unreal.MaterialExpressionSine, x, y, period=1.0), "Input")


def absn(a, x, y):
    return link(a, node(unreal.MaterialExpressionAbs, x, y), "Input")


def mask(a, x, y, r=False, g=False, b=False):
    return link(a, node(unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=False), "Input")


def hard_step(value, threshold, sharp, x, y):
    """saturate((value - threshold) * sharp): anti-aliased toon step."""
    d = sub(value, threshold, x, y)
    m = node(unreal.MaterialExpressionMultiply, x + 1, y, const_b=sharp)
    link(d, m, "A")
    return saturate(m, x + 2, y)


def div(a, b, x, y):
    n = node(unreal.MaterialExpressionDivide, x, y)
    link(a, n, "A")
    link(b, n, "B")
    return n


def frac(a, x, y):
    return link(a, node(unreal.MaterialExpressionFrac, x, y), "Input")


# ---------------------------------------------------------------- parameters
tiling = scalar("NoiseTiling", 0.0045, 0, 0)
flow = scalar("FlowSpeed", 1.0, 0, 1)
deep_col = color("DeepColor", (0.004, 0.02, 0.024), 0, 2)
shallow_col = color("ShallowColor", (0.03, 0.10, 0.09), 0, 3)
ripple_col = color("RippleColor", (0.06, 0.20, 0.18), 0, 4)
rim_col = color("RimColor", (0.12, 0.22, 0.22), 0, 5)
foam_col = color("FoamColor", (0.92, 0.97, 0.95), 0, 6)
shallow_dist = scalar("ShallowDistance", 250.0, 0, 7)
foam_dist = scalar("FoamDistance", 45.0, 0, 8)
foam_noise = scalar("FoamNoiseAmount", 0.35, 0, 9)
ripple_th = scalar("RippleThickness", 0.035, 0, 10)
deep_opacity = scalar("DeepOpacity", 0.95, 0, 11)
shallow_opacity = scalar("ShallowOpacity", 0.6, 0, 12)

# ---------------------------------------------------------------- world-space UV + two flowing layers
world = node(unreal.MaterialExpressionWorldPosition, 1, 0)
uv = mul(mask(world, 2, 0, r=True, g=True), tiling, 3, 0)
time = node(unreal.MaterialExpressionTime, 1, 1)
tflow = mul(time, flow, 2, 1)


def build_layer(coord, speed, row):
    pan = node(unreal.MaterialExpressionPanner, 5, row, speed_x=speed[0], speed_y=speed[1])
    link(coord, pan, "Coordinate")
    link(tflow, pan, "Time")
    # cheap domain warp so the cells look wobbly instead of a grid
    sx = sine(mul(mask(pan, 6, row, g=True), node(unreal.MaterialExpressionConstant, 6, row + 1, r=0.73), 7, row), 8, row)
    sy = sine(mul(mask(pan, 6, row + 2, r=True), node(unreal.MaterialExpressionConstant, 6, row + 3, r=0.61), 7, row + 2), 8, row + 2)
    warp = node(unreal.MaterialExpressionAppendVector, 9, row)
    link(sx, warp, "A")
    link(sy, warp, "B")
    warp_scaled = node(unreal.MaterialExpressionMultiply, 10, row, const_b=0.15)
    link(warp, warp_scaled, "A")
    warped = add(pan, warp_scaled, 11, row)
    s1 = sine(mask(warped, 12, row, r=True), 13, row)
    s2 = sine(mask(warped, 12, row + 1, g=True), 13, row + 1)
    return absn(mul(s1, s2, 14, row), 15, row)  # 0 along cell borders, ~1 inside


cell_a = build_layer(uv, (0.030, 0.020), 3)
uv_b = node(unreal.MaterialExpressionMultiply, 4, 8, const_b=1.9)
link(uv, uv_b, "A")
cell_b = build_layer(uv_b, (-0.025, 0.035), 9)

# ripple lines: thin where either layer is near a cell border
cells = mul(cell_a, cell_b, 16, 6)
ripple = one_minus(hard_step(cells, ripple_th, 40.0, 17, 6), 20, 6)

# ---------------------------------------------------------------- depth based colour + foam
shallow_fade = node(unreal.MaterialExpressionDepthFade, 17, 9)
link(shallow_dist, shallow_fade, "FadeDistance")
shallowness = one_minus(shallow_fade, 18, 9)  # 1 near shore/objects, 0 in the deep

foam_fade = node(unreal.MaterialExpressionDepthFade, 17, 11)
link(foam_dist, foam_fade, "FadeDistance")
proximity = one_minus(foam_fade, 18, 11)

# wobble the foam edge with the cell pattern and pulse it in and out like lapping water
wobble = mul(sub(cell_a, node(unreal.MaterialExpressionConstant, 17, 12, r=0.5), 18, 12), foam_noise, 20, 12)
pulse = mul(sine(mul(time, node(unreal.MaterialExpressionConstant, 17, 13, r=0.15), 18, 13), 20, 13),
            node(unreal.MaterialExpressionConstant, 21, 13, r=0.08), 22, 13)
foam_th = add(node(unreal.MaterialExpressionConstant, 22, 12, r=0.6), pulse, 23, 12)
foam_edge = hard_step(add(proximity, wobble, 21, 11), foam_th, 25.0, 24, 11)
# solid white: no cut-outs, so the water pattern never shows through the foam
foam = foam_edge

# outer foam ring: a second, wider band that stays broken-up, so contact reads as a splash outline
foam_outer_dist = scalar("FoamOuterDistance", 110.0, 0, 13)
outer_fade = node(unreal.MaterialExpressionDepthFade, 17, 15)
link(foam_outer_dist, outer_fade, "FadeDistance")
outer_prox = one_minus(outer_fade, 18, 15)
outer_edge = hard_step(add(outer_prox, wobble, 21, 15), foam_th, 25.0, 24, 15)
outer_ring = saturate(sub(outer_edge, foam_edge, 28, 15), 29, 15)
outer_ring = mul(outer_ring, hard_step(cell_b, node(unreal.MaterialExpressionConstant, 24, 16, r=0.35), 20.0, 25, 16), 30, 15)
# The outer ring is a separate, softer layer. The white foam is composited over it afterwards, so it always wins.
foam_outer = mul(outer_ring, node(unreal.MaterialExpressionConstant, 30, 16, r=0.5), 31, 15)
foam_outer_col = color("OuterFoamColor", (0.42, 0.66, 0.65), 0, 17)
foam_all = node(unreal.MaterialExpressionMax, 33, 15)
link(foam, foam_all, "A")
link(foam_outer, foam_all, "B")

# ---------------------------------------------------------------- vertex waves (world position offset)
wave_height = scalar("WaveHeight", 12.0, 0, 14)
wave_freq = scalar("WaveFrequency", 0.004, 0, 15)
wave_speed = scalar("WaveSpeed", 1.0, 0, 16)
world_xy = mask(world, 2, 20, r=True, g=True)
wtime = mul(time, wave_speed, 3, 21)


def wave(direction, freq_mul, speed_mul, row):
    d = node(unreal.MaterialExpressionConstant2Vector, 3, row, r=direction[0], g=direction[1])
    dot = node(unreal.MaterialExpressionDotProduct, 4, row)
    link(world_xy, dot, "A")
    link(d, dot, "B")
    phase = add(mul(mul(dot, wave_freq, 5, row), node(unreal.MaterialExpressionConstant, 5, row + 1, r=freq_mul), 6, row),
                mul(wtime, node(unreal.MaterialExpressionConstant, 6, row + 1, r=speed_mul), 7, row), 8, row)
    return sine(phase, 9, row)


# two crossing swells plus a small fast one, all along the mesh's Z axis
swell = add(wave((0.8, 0.6), 1.0, 0.18, 22), mul(wave((-0.5, 0.85), 1.7, 0.27, 24),
            node(unreal.MaterialExpressionConstant, 10, 24, r=0.6), 11, 24), 12, 23)
swell = add(swell, mul(wave((0.3, -0.95), 3.1, 0.45, 26),
            node(unreal.MaterialExpressionConstant, 10, 26, r=0.25), 11, 26), 13, 24)
wpo = mul(node(unreal.MaterialExpressionConstant3Vector, 13, 22, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0)),
          mul(swell, wave_height, 14, 24), 15, 23)
mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

# ---------------------------------------------------------------- boat wake
# The boat (ABoatPawn) paints three foam streaks behind its stern into RT_Wake every frame and fades the old ones.
# The map covers a fixed world area (MPC_Water.WakeMap = centre X, centre Y, size), so streaks stay where they were
# painted. The water only reads it: bent a little for a crooked look, thresholded, and chopped into uneven pieces.
MPC_PATH = "/Game/Materials/MPC_Water"
RT_PATH = "/Game/Materials/RT_Wake"
tools = unreal.AssetToolsHelpers.get_asset_tools()

if not eal.does_asset_exist(MPC_PATH):
    tools.create_asset("MPC_Water", "/Game/Materials", unreal.MaterialParameterCollection,
                       unreal.MaterialParameterCollectionFactoryNew())
mpc = eal.load_asset(MPC_PATH)
map_param = unreal.CollectionVectorParameter()
map_param.set_editor_properties({"parameter_name": "WakeMap", "default_value": unreal.LinearColor(0.0, 0.0, 10000.0, 0.0)})
mpc.set_editor_property("vector_parameters", [map_param])
mpc.set_editor_property("scalar_parameters", [])
eal.save_asset(MPC_PATH)

if not eal.does_asset_exist(RT_PATH):
    tools.create_asset("RT_Wake", "/Game/Materials", unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
wake_rt = eal.load_asset(RT_PATH)
wake_rt.set_editor_properties({
    "size_x": 2048, "size_y": 2048,
    "render_target_format": unreal.TextureRenderTargetFormat.RTF_RGBA16F,
    "clear_color": unreal.LinearColor(0.0, 0.0, 0.0, 1.0),
    "address_x": unreal.TextureAddress.TA_CLAMP, "address_y": unreal.TextureAddress.TA_CLAMP,
    "auto_generate_mips": False,
})
eal.save_asset(RT_PATH)


def const(value, x, y):
    return node(unreal.MaterialExpressionConstant, x, y, r=value)


def div(a, b, x, y):
    n = node(unreal.MaterialExpressionDivide, x, y)
    link(a, n, "A")
    link(b, n, "B")
    return n


wake_intensity = scalar("WakeIntensity", 1.0, 0, 18)
wake_col = color("WakeColor", (0.80, 0.93, 0.91), 0, 19)

wake_map = node(unreal.MaterialExpressionCollectionParameter, 34, 20, collection=mpc, parameter_name="WakeMap")
map_xy = mask(wake_map, 35, 20, r=True, g=True)
map_size = mask(wake_map, 35, 21, b=True)
half = node(unreal.MaterialExpressionConstant2Vector, 36, 22, r=0.5, g=0.5)
map_uv = add(div(sub(world_xy, map_xy, 36, 20), map_size, 37, 20), half, 38, 20)

# crooked: bend the lookup a little with two slow, drifting sines (about 12 cm)
wx = mask(world_xy, 36, 23, r=True)
wy = mask(world_xy, 36, 24, g=True)
bend = node(unreal.MaterialExpressionAppendVector, 41, 23)
link(mul(sine(add(mul(wy, const(0.0021, 37, 23), 38, 23), mul(time, const(0.05, 37, 24), 38, 24), 39, 23), 40, 23),
         const(0.0012, 40, 24), 41, 24), bend, "A")
link(mul(sine(add(mul(wx, const(0.0017, 37, 25), 38, 25), mul(time, const(-0.04, 37, 26), 38, 26), 39, 25), 40, 25),
         const(0.0012, 40, 26), 41, 26), bend, "B")
bent_uv = add(map_uv, bend, 42, 20)

wake_tex = node(unreal.MaterialExpressionTextureSample, 43, 20, texture=wake_rt,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
link(bent_uv, wake_tex, "UVs")
wake_raw = mask(wake_tex, 44, 20, r=True)


def minimum(a, b, x, y):
    n = node(unreal.MaterialExpressionMin, x, y)
    link(a, n, "A")
    link(b, n, "B")
    return n


# Nothing outside the map, and a soft fade at its border: the texture is clamped, so foam on the border pixels would
# otherwise be stretched into straight lines across the rest of the sea.
uv_u = mask(bent_uv, 43, 27, r=True)
uv_v = mask(bent_uv, 43, 28, g=True)
edge = minimum(minimum(uv_u, one_minus(uv_u, 44, 27), 45, 27), minimum(uv_v, one_minus(uv_v, 44, 28), 45, 28), 46, 27)
edge_mask = saturate(mul(edge, const(30.0, 46, 28), 47, 27), 48, 27)
wake_value = mul(wake_raw, edge_mask, 45, 20)

# Solid foam: a streak is either there or not. As it fades it gets thinner and breaks up (higher threshold) instead
# of turning transparent, so the water pattern never shows through it.
chop = saturate(mul(sub(cell_b, const(0.12, 45, 23), 46, 23), const(4.0, 45, 24), 47, 23), 48, 23)
wake_th = add(const(0.14, 48, 24), mul(one_minus(chop, 48, 25), const(0.3, 48, 26), 49, 25), 50, 24)
wake_mask = saturate(mul(mul(sub(wake_value, wake_th, 51, 20), const(7.0, 51, 21), 52, 20), wake_intensity, 53, 20), 54, 20)

# ---------------------------------------------------------------- foam coverage
foam_any = foam_all

# where any foam is, the water highlights are switched off, so nothing from the sea shows on top of the foam
foam_cover = node(unreal.MaterialExpressionMax, 66, 20)
link(foam_any, foam_cover, "A")
link(wake_mask, foam_cover, "B")
highlight_gate = one_minus(saturate(foam_cover, 67, 20), 68, 20)

# ---------------------------------------------------------------- final colour
base = lerp(deep_col, shallow_col, shallowness, 21, 8)
crest = saturate(swell, 21, 7)  # slightly brighter wave crests
base = add(base, mul(ripple_col, mul(mul(crest, highlight_gate, 21, 10), node(unreal.MaterialExpressionConstant, 21, 9, r=0.25), 22, 7), 23, 8), 24, 8)
with_ripples = add(base, mul(ripple_col, mul(mul(ripple, highlight_gate, 21, 5), node(unreal.MaterialExpressionConstant, 21, 6, r=0.7), 22, 6), 23, 6), 24, 7)
fresnel = node(unreal.MaterialExpressionFresnel, 21, 4, exponent=3.0)
rim = mul(rim_col, fresnel, 22, 4)
lit = add(with_ripples, rim, 25, 6)
with_outer = lerp(lit, foam_outer_col, foam_outer, 29, 7)
with_wake = lerp(with_outer, wake_col, wake_mask, 30, 7)
final = lerp(with_wake, foam_col, foam, 31, 8)  # white foam last, at full strength

opacity = lerp(deep_opacity, shallow_opacity, shallowness, 24, 9)
final_opacity = node(unreal.MaterialExpressionMax, 29, 10)
link(opacity, final_opacity, "A")
link(foam_any, final_opacity, "B")
with_wake_opacity = node(unreal.MaterialExpressionMax, 30, 10)
link(final_opacity, with_wake_opacity, "A")
link(wake_mask, with_wake_opacity, "B")
final_opacity = with_wake_opacity

mel.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(final_opacity, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
eal.save_asset(mat_path)
try:
    stats = mel.get_statistics(mat)
    unreal.log(f"[Lake] instructions: VS={stats.num_vertex_shader_instructions} PS={stats.num_pixel_shader_instructions}, "
               f"samplers={stats.num_samplers}")
except Exception as e:  # statistics are informational only
    unreal.log_warning(f"[Lake] no statistics: {e}")

# ---------------------------------------------------------------- assign to DenizMesh
mesh = eal.load_asset(MESH_PATH)
unreal.log(f"[Lake] mesh {MESH_PATH} is {type(mesh).__name__}")
if isinstance(mesh, unreal.StaticMesh):
    for i in range(len(mesh.static_materials)):
        mesh.set_material(i, mat)
    unreal.log(f"[Lake] assigned to {len(mesh.static_materials)} static mesh slot(s)")
elif isinstance(mesh, unreal.SkeletalMesh):
    slots = mesh.materials
    for i in range(len(slots)):
        slots[i].material_interface = mat
    mesh.set_editor_property("materials", slots)
else:
    raise RuntimeError("DenizMesh is neither a StaticMesh nor a SkeletalMesh")
eal.save_asset(MESH_PATH)
unreal.log("[Lake] done")
