import unreal

W = unreal.WidgetService
P = '/Game/UI/WBP_HullStatus'
eal = unreal.EditorAssetLibrary

if eal.does_asset_exist(P):
    eal.delete_asset(P)
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/Deneme.FishHullWidget'))
unreal.AssetToolsHelpers.get_asset_tools().create_asset('WBP_HullStatus', '/Game/UI', unreal.WidgetBlueprint, factory)

print('HULL', W.add_component(P, 'CanvasPanel', 'RootCanvas', '', False).success)
print('HULL', W.add_component(P, 'CanvasPanel', 'HullRoot', 'RootCanvas', True).success)

# (centre x, centre y, width, height, angle) inside a 150 x 210 box: a boat seen from above, made of planks.
# Seg_0 is the first plank to be lost, Seg_5 (the keel) the last.
PLANKS = {
    'Seg_0': (48, 115, 9, 84, 0),      # left side
    'Seg_1': (90, 50, 9, 52, -28),     # right bow
    'Seg_2': (102, 115, 9, 84, 0),     # right side
    'Seg_3': (60, 50, 9, 52, 28),      # left bow
    'Seg_4': (75, 160, 56, 9, 0),      # stern
    'Seg_5': (75, 108, 5, 76, 0),      # keel
}
for name in PLANKS:
    print('HULL', name, W.add_component(P, 'Border', name, 'HullRoot', True).success)

base = f'{P}.WBP_HullStatus:WidgetTree.'


def anchor(slot, x, y, w, h, alignment, anchors):
    layout = slot.get_editor_property('layout_data')
    a = layout.get_editor_property('anchors')
    a.set_editor_property('minimum', unreal.Vector2D(*anchors))
    a.set_editor_property('maximum', unreal.Vector2D(*anchors))
    layout.set_editor_property('anchors', a)
    layout.set_editor_property('alignment', unreal.Vector2D(*alignment))
    layout.set_editor_property('offsets', unreal.Margin(x, y, w, h))
    slot.set_editor_property('layout_data', layout)


# the icon sits in the bottom-left corner of the screen
root = unreal.load_object(None, base + 'HullRoot')
anchor(root.get_editor_property('slot'), 48, -40, 150, 210, (0.0, 1.0), (0.0, 1.0))

for name, (cx, cy, w, h, angle) in PLANKS.items():
    border = unreal.load_object(None, base + name)
    anchor(border.get_editor_property('slot'), cx, cy, w, h, (0.5, 0.5), (0.0, 0.0))
    background = border.get_editor_property('background')
    background.set_editor_property('draw_as', unreal.SlateBrushDrawType.ROUNDED_BOX)
    outline = background.get_editor_property('outline_settings')
    outline.set_editor_property('corner_radii', unreal.Vector4(5, 5, 5, 5))
    outline.set_editor_property('rounding_type', unreal.SlateBrushRoundingType.FIXED_RADIUS)
    background.set_editor_property('outline_settings', outline)
    border.set_editor_property('background', background)
    border.set_editor_property('brush_color', unreal.LinearColor(0.9, 0.96, 0.94, 0.95))
    transform = unreal.WidgetTransform()
    transform.set_editor_property('angle', float(angle))
    border.set_editor_property('render_transform', transform)

unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset(P))
eal.save_asset(P)
print('HULL valid', W.validate(P))

# show it through the HUD blueprint
hud = unreal.load_asset('/Game/Blueprints/BP_FishHUD')
unreal.get_default_object(hud.generated_class()).set_editor_property('hull_widget_class', unreal.load_asset(P).generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(hud)
eal.save_asset('/Game/Blueprints/BP_FishHUD')
print('HULL hud', unreal.get_default_object(hud.generated_class()).get_editor_property('hull_widget_class'))
