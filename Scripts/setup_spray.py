import unreal

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

MAT = "/Game/Materials/M_Spray"
if not eal.does_asset_exist(MAT):
    mat = at.create_asset("M_Spray", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    col.set_editor_property("constant", unreal.LinearColor(1.6, 1.8, 1.8, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(MAT)

bp = unreal.load_asset("/Game/Blueprints/BP_Harpoon")
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("spray_material", unreal.load_asset(MAT))
eal.save_asset("/Game/Blueprints/BP_Harpoon")
print("spray setup done")
