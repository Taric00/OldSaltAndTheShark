import unreal

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# --- rope material: dark tarred rope, rough
MAT = "/Game/Materials/M_Rope"
if not eal.does_asset_exist(MAT):
    mat = at.create_asset("M_Rope", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    col.set_editor_property("constant", unreal.LinearColor(0.16, 0.11, 0.07, 1.0))
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 150)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(MAT)
rope_mat = unreal.load_asset(MAT)

# --- BP_Harpoon
BP = "/Game/Blueprints/BP_Harpoon"
if not eal.does_asset_exist(BP):
    fac = unreal.BlueprintFactory()
    fac.set_editor_property("parent_class", unreal.load_class(None, "/Script/Deneme.Harpoon"))
    at.create_asset("BP_Harpoon", "/Game/Blueprints", unreal.Blueprint, fac)
bp = unreal.load_asset(BP)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("rope_material", rope_mat)
tip = unreal.load_asset(MAT)
eal.save_asset(BP)

# --- hand it to the character
chbp = unreal.load_asset("/Game/Blueprints/BP_FishPhysicsCharacter")
ch_cdo = unreal.get_default_object(chbp.generated_class())
ch_cdo.set_editor_property("harpoon_class", bp.generated_class())
eal.save_asset("/Game/Blueprints/BP_FishPhysicsCharacter")
print("harpoon setup done")
