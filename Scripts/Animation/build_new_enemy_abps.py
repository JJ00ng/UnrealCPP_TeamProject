"""Create standalone UE 5.5 enemy ABPs using the project's ABP_Tree graph.

Run with UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<this file>
    -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities,NewEnemyAnimationTools
    -unattended -NullRHI

Existing destination assets are never overwritten. Set NEW_ENEMY_VALIDATE=1 to
validate saved assets in a fresh editor process without creating anything.
"""

import json
import os
from pathlib import Path
import unreal

ROOT = '/Game/Assets/NewEnemy'
DESTINATION = '/Game/Enemy/Animation/ABP/NewEnemy'
TEMPLATE = '/Game/Enemy/Animation/ABP/ABP_Tree'
STATE_MACHINE = ':AnimGraph.AnimGraphNode_StateMachine_0.New State Machine'
IDLE_NODE = STATE_MACHINE + '.AnimStateNode_1.Idle.AnimGraphNode_SequencePlayer_0'
MOVE_NODE = STATE_MACHINE + '.AnimStateNode_0.Move.AnimGraphNode_SequencePlayer_2'
SLOT_NODE = ':AnimGraph.AnimGraphNode_Slot_0'


def definition(name, mesh, skeleton, idle, move):
    return dict(name=name, mesh=ROOT + '/' + mesh, skeleton=ROOT + '/' + skeleton,
                idle=ROOT + '/' + idle, move=ROOT + '/' + move)


DEFINITIONS = [
    definition('Kraken', 'Kraken/Meshes/KRAKEN', 'Kraken/Meshes/KRAKEN_Skeleton',
               'Kraken/Animations/KRAKEN_idle', 'Kraken/Animations/KRAKEN_walk'),
    definition('Sevarog', 'Sevarog/Meshes/Sevarog', 'Sevarog/Meshes/Sevarog_Skeleton',
               'Sevarog/Animations/Idle', 'Sevarog/Animations/Run_Fwd'),
    definition('ShadowKnight', 'ShadowKight/Mesh/ShadowknightUE_SK',
               'ShadowKight/Mesh/ShadowknightUE_Skeleton',
               'ShadowKight/Animations/ThirdPersonIdle', 'ShadowKight/Animations/ThirdPersonRun'),
    definition('StoneGolem', 'StoneGolem/Mesh/SKM_StoneGolem',
               'StoneGolem/Mesh/SKEL_StoneGolem_Skeleton',
               'StoneGolem/Animations/AS_StoneGolem_idle1', 'StoneGolem/Animations/AS_StoneGolem_jogF'),
]

for tree in ['Ash', 'Birch', 'Oak']:
    base = 'PackOfTreeEnts/TreeEnt' + tree
    for version in ['UE4', 'UE5']:
        DEFINITIONS.append(definition(
            'TreeEnt' + tree + '_' + version,
            base + '/Meshes/' + version + '/SK_TreeEnt' + tree + '_' + version,
            base + '/Meshes/' + version + '/SKEL_TreeEnt' + tree + '_Skeleton_' + version,
            base + '/Animations/' + version + '/Anim_Idle1',
            base + '/Animations/' + version + '/Anim_Run'))

DEFINITIONS.append(definition(
    'TreeEntSpruce', 'PackOfTreeEnts/TreeEntSpruce/Meshes/SK_TreeEntSpruce',
    'PackOfTreeEnts/TreeEntSpruce/Meshes/SKEL_TreeEntSpruce_Skeleton',
    'PackOfTreeEnts/TreeEntSpruce/Animations/Anim_Idle1',
    'PackOfTreeEnts/TreeEntSpruce/Animations/Anim_Run'))


def load(path, expected_type):
    asset = unreal.load_asset(path)
    if not isinstance(asset, expected_type):
        raise RuntimeError('Missing or incorrect asset type: ' + path)
    return asset


def graph_node(bp, suffix):
    obj = unreal.find_object(None, bp.get_path_name() + suffix)
    if obj is None:
        raise RuntimeError('Graph node not found: ' + bp.get_path_name() + suffix)
    return obj


def configure_character_owner(bp, mesh):
    # The reference graph casts to EliteEnemy. New rigs must also work on
    # normal enemies and bosses, all of which derive from Character.
    if not unreal.NewEnemyAnimationLibrary.configure_character_owner(bp, mesh):
        raise RuntimeError('Could not configure Character owner: ' + bp.get_path_name())


def export_text(asset, path):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(path)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError('Could not export ' + asset.get_path_name())
    raw = path.read_bytes()
    return raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')


def validate(bp, spec, output):
    skeleton = load(spec['skeleton'], unreal.Skeleton)
    mesh = load(spec['mesh'], unreal.SkeletalMesh)
    assert mesh.get_editor_property('skeleton') == skeleton, spec['name'] + ': mesh/skeleton mismatch'
    assert bp.get_editor_property('target_skeleton') == skeleton, spec['name'] + ': ABP/skeleton mismatch'
    for suffix, key in [(IDLE_NODE, 'idle'), (MOVE_NODE, 'move')]:
        node = graph_node(bp, suffix).get_editor_property('node')
        sequence = node.get_editor_property('sequence')
        assert sequence == load(spec[key], unreal.AnimSequence), spec['name'] + ': incorrect ' + key
        assert sequence.get_editor_property('skeleton') == skeleton, spec['name'] + ': incompatible sequence'
        assert node.get_editor_property('loop_animation'), spec['name'] + ': non-looping locomotion'
        assert sequence.get_play_length() > 0, spec['name'] + ': empty sequence'
        assert sequence.get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_NONE
    assert str(graph_node(bp, SLOT_NODE).get_editor_property('node').get_editor_property('slot_name')) == 'DefaultSlot'
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    generated = unreal.BlueprintEditorLibrary.generated_class(bp)
    assert generated is not None, spec['name'] + ': missing generated class'
    text = export_text(bp, output / ('ABP_' + spec['name'] + '.copy'))
    assert 'GroundSpeed' in text and 'ShouldMove' in text and 'BlueprintUpdateAnimation' in text
    assert '/Game/Enemy/Animation/Tree/' not in text, spec['name'] + ': old animation reference'
    assert '/Game/Enemy/MonsterElite/Tree/' not in text, spec['name'] + ': old skeleton reference'
    assert 'TargetType="/Script/CoreUObject.Class\'/Script/Engine.Character\'"' in text
    assert 'PinName="AsCharacter"' in text
    assert 'PinName="AsEliteEnemy"' not in text
    return dict(spec, abp=bp.get_path_name(), result='passed')


def main():
    output = Path(unreal.Paths.project_saved_dir()) / 'Codex' / 'NewEnemy'
    output.mkdir(parents=True, exist_ok=True)
    validate_only = os.environ.get('NEW_ENEMY_VALIDATE') == '1'
    template = load(TEMPLATE, unreal.AnimBlueprint)
    # Validate every source before creating any assets.
    for spec in DEFINITIONS:
        for field, cls in [('mesh', unreal.SkeletalMesh), ('skeleton', unreal.Skeleton),
                           ('idle', unreal.AnimSequence), ('move', unreal.AnimSequence)]:
            load(spec[field], cls)
        target_path = DESTINATION + '/ABP_' + spec['name']
        if not validate_only and unreal.EditorAssetLibrary.does_asset_exist(target_path):
            raise RuntimeError('Refusing to overwrite existing ABP: ' + target_path)

    report = []
    for spec in DEFINITIONS:
        target_path = DESTINATION + '/ABP_' + spec['name']
        if validate_only:
            bp = load(target_path, unreal.AnimBlueprint)
        else:
            bp = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
                'ABP_' + spec['name'], DESTINATION, template)
            if bp is None:
                raise RuntimeError('Could not duplicate ' + target_path)
            # Remap graph assets before changing skeleton. No animation data is
            # retargeted; each graph uses animations authored for its own rig.
            for suffix, field in [(IDLE_NODE, 'idle'), (MOVE_NODE, 'move')]:
                obj = graph_node(bp, suffix)
                node = obj.get_editor_property('node')
                node.set_editor_property('sequence', load(spec[field], unreal.AnimSequence))
                node.set_editor_property('loop_animation', True)
                obj.set_editor_property('node', node)
            bp.set_editor_property('target_skeleton', load(spec['skeleton'], unreal.Skeleton))
            configure_character_owner(bp, load(spec['mesh'], unreal.SkeletalMesh))
            bp.set_editor_property('blueprint_description',
                'NewEnemy ' + spec['name'] + '. Based on ABP_Tree: Idle/Move and DefaultSlot. '
                'Assign to the matching Character mesh; attack/death montages use DefaultSlot.')
        report.append(validate(bp, spec, output))
        if not validate_only and not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
            raise RuntimeError('Could not save ' + target_path)
        unreal.log('NEW_ENEMY_OK: ' + target_path)

    filename = 'validation.json' if validate_only else 'creation.json'
    (output / filename).write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('NEW_ENEMY_COMPLETE: ' + str(len(report)) + ' ABPs')


if __name__ == '__main__':
    main()
