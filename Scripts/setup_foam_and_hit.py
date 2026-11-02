import unreal, runpy

eal = unreal.EditorAssetLibrary

# The water mesh must not be part of the distance field the foam reads, or the whole sea would count as "near a surface".
mesh = eal.load_asset('/Game/Models/DenizMesh')
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
if sub:
    settings = sub.get_lod_build_settings(mesh, 0)
    settings.set_editor_property('distance_field_resolution_scale', 0.0)
    sub.set_lod_build_settings(mesh, 0, settings)
    eal.save_asset('/Game/Models/DenizMesh')
    print('FOAM mesh df scale', sub.get_lod_build_settings(mesh, 0).get_editor_property('distance_field_resolution_scale'))

runpy.run_path(r'C:/Users/albay/Documents/Unreal Projects/Deneme/Scripts/setup_wake.py')
runpy.run_path(r'C:/Users/albay/Documents/Unreal Projects/Deneme/Scripts/create_hit_effect.py')

hit_material = eal.load_asset('/Game/Materials/M_PP_HitDizzy')
for path in ('/Game/Blueprints/BP_Boat', '/Game/Blueprints/BP_FishPhysicsCharacter'):
    bp = unreal.load_asset(path)
    unreal.get_default_object(bp.generated_class()).set_editor_property('hit_effect_material', hit_material)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_asset(path)
    print('HITFX assigned', path)
