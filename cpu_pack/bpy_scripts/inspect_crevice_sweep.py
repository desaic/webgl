"""Background Blender comparison of the pack and radius-sweep candidate patches.

blender --background --python inspect_crevice_sweep.py -- <sweep-folder>
Writes an inspectable .blend and consistently framed PNGs into that folder.
Uses load_pack.py's pack parsing and shared mesh instances; never edits a pack.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def import_obj(path, collection, name, color):
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis='Y', up_axis='Z')
    objects = list(bpy.context.selected_objects)
    if not objects:
        raise RuntimeError(f'No geometry imported from {path}')
    for obj in objects:
        for old in list(obj.users_collection):
            old.objects.unlink(obj)
        collection.objects.link(obj)
        obj.name = name
        obj.color = color
    return objects


def main():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    folder = Path(args[0] if args else 'F:/meshes/fruit_hand/crevice_radius_sweep')
    dataset = folder.parent
    spec = importlib.util.spec_from_file_location('pack_reader', Path(__file__).with_name('load_pack.py'))
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    instances = reader.load_pack(str(dataset / 'pack_filtered_step2.txt'))
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    base_scene = bpy.context.scene
    base_scene.name = 'Original pack'
    support = bpy.data.collections.new('Fixed pack - shared mesh instances')
    base_scene.collection.children.link(support)
    meshes = {}
    low = Vector((float('inf'),) * 3)
    high = Vector((-float('inf'),) * 3)
    for index, inst in enumerate(instances):
        name = inst['name']
        if name not in meshes:
            imported = import_obj(dataset / 'fruits_1' / (name + '.obj'), support,
                                  name, (0.52, 0.55, 0.6, 1))
            if len(imported) != 1:
                raise RuntimeError(f'Expected one object in fruit asset {name}')
            obj = imported[0]
            meshes[name] = obj.data
        else:
            obj = bpy.data.objects.new(f'{name}_{index}', meshes[name])
            support.objects.link(obj)
        obj.name = f'{name}_{index}'
        obj.color = (0.52, 0.55, 0.6, 1)
        reader.apply_transformation(obj, *inst['transform'])
    bpy.context.view_layer.update()
    for obj in support.objects:
        for corner in obj.bound_box:
            p = obj.matrix_world @ Vector(corner)
            for axis in range(3):
                low[axis] = min(low[axis], p[axis])
                high[axis] = max(high[axis], p[axis])
    center = (low + high) / 2
    extent = (high - low).length
    camera_data = bpy.data.cameras.new('Shared comparison camera')
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = max(high - low) * 1.14
    camera_data.clip_end = extent * 10
    camera = bpy.data.objects.new('Shared comparison camera', camera_data)
    camera.location = center + Vector((0.2, -1, 0.5)).normalized() * extent * 2
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    scenes = [(base_scene, 'original_pack')]
    counts = {'pack_instances': len(support.objects), 'unique_meshes': len(meshes), 'radii': {}}
    for radius in (3, 6, 12):
        scene = bpy.data.scenes.new(f'Radius {radius} cm - orange candidate patches')
        scene.collection.children.link(support)
        collection = bpy.data.collections.new(f'Radius {radius} cm diagnostics')
        scene.collection.children.link(collection)
        bpy.context.window.scene = scene
        stem = f'r{radius}_dx1'
        patches = import_obj(folder / (stem + '_patches.obj'), collection,
                             'Candidate mouth patches', (1, 0.3, 0.035, 1))
        counts['radii'][str(radius)] = sum(len(o.data.polygons) for o in patches)
        for suffix, title, color in (
            ('_hull.obj', 'Shrinkwrap - hidden for comparison', (0.2, 0.5, 0.85, 1)),
            ('_new_patches.obj', 'New vs radius 3 - toggle to inspect', (0.1, 0.85, 0.3, 1)),
        ):
            path = folder / (stem + suffix)
            if path.stat().st_size == 0:
                continue
            # The baseline's new-patch OBJ is intentionally empty.
            if radius == 3 and suffix == '_new_patches.obj':
                continue
            for obj in import_obj(path, collection, title, color):
                obj.hide_render = True
                obj.hide_set(True)
        scenes.append((scene, stem))
    for scene, filename in scenes:
        scene.collection.objects.link(camera)
        scene.camera = camera
        scene.render.engine = 'BLENDER_WORKBENCH'
        scene.render.resolution_x = 1600
        scene.render.resolution_y = 1000
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = 'PNG'
        scene.display.shading.light = 'STUDIO'
        scene.display.shading.color_type = 'OBJECT'
        scene.display.shading.show_shadows = True
        scene.display.shading.show_cavity = True
        scene.display.shading.background_type = 'WORLD'
        scene.world = base_scene.world
        scene.world.color = (0.055, 0.055, 0.055)
        scene.render.filepath = str(folder / (filename + '.png'))
        bpy.context.window.scene = scene
        bpy.ops.render.render(write_still=True)
    bpy.context.window.scene = scenes[-1][0]
    bpy.ops.wm.save_as_mainfile(filepath=str(folder / 'crevice_radius_sweep.blend'))
    (folder / 'blender_counts.json').write_text(json.dumps(counts, indent=2))


if __name__ == '__main__':
    main()
