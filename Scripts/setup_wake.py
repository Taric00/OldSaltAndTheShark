import unreal, runpy

runpy.run_path(r'C:/Users/albay/Documents/Unreal Projects/Deneme/Scripts/create_lake_material.py')

bp = unreal.load_asset('/Game/Blueprints/BP_Boat')
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('water_parameters', unreal.load_asset('/Game/Materials/MPC_Water'))
cdo.set_editor_property('wake_render_target', unreal.load_asset('/Game/Materials/RT_Wake'))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset('/Game/Blueprints/BP_Boat')
print('WAKE boat', cdo.get_editor_property('wake_render_target'), cdo.get_editor_property('water_parameters'), len(cdo.get_editor_property('wake_lines')))
