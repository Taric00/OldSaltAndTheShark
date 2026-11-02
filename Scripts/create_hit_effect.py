"""Creates M_PP_HitDizzy, a post process material: wobbling, colour-split, darkened-edge 'dizzy' screen.
Driven by one scalar parameter, Intensity (0 = off, 1 = full). The pawns' hit effect writes it each frame."""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
PATH = "/Game/Materials/M_PP_HitDizzy"

if eal.does_asset_exist(PATH):
    mat = eal.load_asset(PATH)
    mel.delete_all_material_expressions(mat)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_PP_HitDizzy", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())

mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x * 220, y * 160)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(src, dst, pin):
    if not (mel.connect_material_expressions(src, "", dst, pin) or (pin == "Input" and mel.connect_material_expressions(src, "", dst, ""))):
        raise RuntimeError(f"connect failed: {src.get_name()} -> {dst.get_name()}.{pin}")
    return dst


def two(cls, a, b, x, y, pa="A", pb="B"):
    n = node(cls, x, y)
    link(a, n, pa)
    link(b, n, pb)
    return n


def mul(a, b, x, y):
    return two(unreal.MaterialExpressionMultiply, a, b, x, y)


def add(a, b, x, y):
    return two(unreal.MaterialExpressionAdd, a, b, x, y)


def sub(a, b, x, y):
    return two(unreal.MaterialExpressionSubtract, a, b, x, y)


def const(v, x, y):
    return node(unreal.MaterialExpressionConstant, x, y, r=v)


def mask(a, x, y, r=False, g=False, b=False):
    return link(a, node(unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=False), "Input")


def saturate(a, x, y):
    return link(a, node(unreal.MaterialExpressionSaturate, x, y), "Input")


def sine(a, x, y):
    return link(a, node(unreal.MaterialExpressionSine, x, y, period=1.0), "Input")


def append(a, b, x, y):
    return two(unreal.MaterialExpressionAppendVector, a, b, x, y)


intensity = node(unreal.MaterialExpressionScalarParameter, 0, 0, parameter_name="Intensity", default_value=0.0)
time = node(unreal.MaterialExpressionTime, 0, 1)
uv = node(unreal.MaterialExpressionTextureCoordinate, 0, 2)

u = mask(uv, 1, 2, r=True)
v = mask(uv, 1, 3, g=True)
centre = sub(uv, node(unreal.MaterialExpressionConstant2Vector, 1, 4, r=0.5, g=0.5), 2, 2)
radius = link(two(unreal.MaterialExpressionDotProduct, centre, centre, 3, 2), node(unreal.MaterialExpressionSquareRoot, 4, 2), "Input")

# the picture sways: two slow sines bend the lookup, more towards the edges and as the effect gets stronger
sway = append(
    sine(add(mul(v, const(9.0, 2, 5), 3, 5), mul(time, const(0.9, 2, 6), 3, 6), 4, 5), 5, 5),
    sine(add(mul(u, const(8.0, 2, 7), 3, 7), mul(time, const(-0.8, 2, 8), 3, 8), 4, 7), 5, 7), 6, 5)
strength = mul(mul(intensity, const(0.010, 5, 9), 6, 9), add(const(0.5, 6, 10), mul(radius, const(1.6, 6, 11), 7, 11), 8, 10), 9, 9)
offset = mul(sway, strength, 10, 5)

# red, green and blue are read from slightly different places: the colours drift apart
channels = []
for index, (amount, pick) in enumerate(((1.0, "r"), (1.5, "g"), (2.0, "b"))):
    lookup = add(uv, mul(offset, const(amount, 11, 12 + index), 12, 12 + index), 13, 12 + index)
    scene = node(unreal.MaterialExpressionSceneTexture, 14, 12 + index, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, filtered=False)
    link(lookup, scene, "UVs")
    channels.append(mask(scene, 15, 12 + index, r=(pick == "r"), g=(pick == "g"), b=(pick == "b")))
colour = append(append(channels[0], channels[1], 16, 12), channels[2], 17, 12)

# washed out, dimmer, and the edges close in
desat = node(unreal.MaterialExpressionDesaturation, 18, 12)
link(colour, desat, "Input")
link(mul(intensity, const(0.55, 17, 14), 18, 14), desat, "Fraction")

edge = saturate(mul(sub(radius, const(0.26, 6, 16), 7, 16), const(2.5, 6, 17), 8, 16), 9, 16)
darkness = saturate(mul(mul(edge, edge, 10, 16), mul(intensity, const(1.55, 10, 18), 11, 18), 12, 16), 13, 16)
dim = sub(const(1.0, 12, 19), mul(intensity, const(0.28, 11, 19), 12, 20), 13, 19)
result = mul(mul(desat, sub(const(1.0, 18, 16), darkness, 19, 16), 20, 12), dim, 21, 12)

mel.connect_material_property(result, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
eal.save_asset(PATH)
print("HITFX material ok", mat)
