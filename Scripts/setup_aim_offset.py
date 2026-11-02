import unreal

ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Game/Characters'], True)
S = unreal.AnimGraphService
B = unreal.BlueprintService
ABP = '/Game/Characters/Animations/ABP_Fish'
AO_PATH = '/Game/Characters/Mannequins/Anims/Rifle/AIM/AO_Rifle'

# 1) Aim offset axes in degrees: yaw -90..90, pitch -60..60 (the camera's own pitch limits)
ao = unreal.load_asset(AO_PATH)


def axis(name, lo, hi):
    p = unreal.BlendParameter()
    p.set_editor_properties({'display_name': name, 'min': lo, 'max': hi, 'grid_num': 4, 'snap_to_grid': False, 'wrap_input': False})
    return p


ao.set_editor_property('blend_parameters', [axis('Yaw', -90.0, 90.0), axis('Pitch', -60.0, 60.0), axis('None', 0.0, 100.0)])
new_samples = []
for s in ao.get_editor_property('sample_data'):
    anim = s.get_editor_property('animation')
    y = {'MM_Rifle_Idle_ADS_AO_CU': 60.0, 'MM_Rifle_Idle_ADS_AO_CD': -60.0}.get(anim.get_name(), 0.0)
    n = unreal.BlendSample()
    n.set_editor_properties({'animation': anim, 'sample_value': unreal.Vector(0.0, y, 0.0), 'rate_scale': 1.0, 'use_single_frame_for_blending': False})
    new_samples.append(n)
ao.set_editor_property('sample_data', new_samples)
unreal.EditorAssetLibrary.save_asset(AO_PATH)
print('SETUP AO', [(s.get_editor_property('animation').get_name(), s.get_editor_property('sample_value').y) for s in ao.get_editor_property('sample_data')],
      [ao.get_editor_property('blend_parameters')[i].export_text()[:60] for i in range(2)])

# 2) Aiming state: rifle blend space -> aim offset -> output
unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset(ABP))
nodes = B.get_nodes_in_graph(ABP, 'Aiming')
bs_id = [n.node_id for n in nodes if 'BS_Rifle_Aim' in n.node_title][0]
out_id = S.get_output_pose_node_id(ABP, 'Aiming')
for n in nodes:
    if 'AimOffset' in n.node_title or 'Aim Offset' in n.node_title:
        B.delete_node(ABP, 'Aiming', n.node_id)

ao_id = B.create_node_by_key(ABP, 'Aiming', "SPAWN AnimGraphNode_RotationOffsetBlendSpace|AimOffset Player 'AO_Rifle'", 450, 0)
print('SETUP connect pose', S.connect_anim_nodes(ABP, 'Aiming', bs_id, 'Pose', ao_id, 'BasePose'))
print('SETUP connect out', S.connect_anim_nodes(ABP, 'Aiming', ao_id, 'Pose', out_id, 'Result'))


def getter(var, x, y):
    n = B.create_node_by_key(ABP, 'Aiming', f'SPAWN K2Node_VariableGet|Get {var}', x, y)
    assert n, var
    return n


yaw = getter('AimYaw', 100, 200)
pitch = getter('AimPitch', 100, 300)
print('SETUP x', B.connect_nodes(ABP, 'Aiming', yaw, 'AimYaw', ao_id, 'X'))
print('SETUP y', B.connect_nodes(ABP, 'Aiming', pitch, 'AimPitch', ao_id, 'Y'))
print('SETUP validate', S.validate_state_machine(ABP, 'Locomotion'))
unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset(ABP))
unreal.EditorAssetLibrary.save_asset(ABP)
print('SETUP nodes', [(n.node_id[:6], n.node_title.replace('\n', ' | ')) for n in B.get_nodes_in_graph(ABP, 'Aiming')])
