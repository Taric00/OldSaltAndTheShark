import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Game/Characters'], True)
P = '/Game/Characters/Animations/ABP_Fish'
for t in ['AO_Rifle', 'Aim Offset Blend Space', 'RotationOffset', 'Aim Offset Player', 'Aim Offset']:
    for n in unreal.BlueprintService.discover_nodes(P, t, '', 30):
        print('PROBE node', t, '|', n.spawner_key)
print('PROBE classes', [c for c in dir(unreal) if 'RotationOffset' in c or 'AimOffset' in c])
