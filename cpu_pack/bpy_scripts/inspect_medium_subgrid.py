"""Render the resampled medium-crevice graph debug export in Blender.

blender --background --python inspect_medium_subgrid.py -- <debug-folder>
Writes medium_subgrid.blend and medium_subgrid.png beside the OBJ/CSV files.
"""

import csv
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def import_lines(path, name, color, thickness):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis="Y", up_axis="Z")
    objects = list(bpy.context.selected_objects)
    for obj in objects:
        obj.name = name
        obj.color = color
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.ops.object.convert(target="CURVE")
        curve = bpy.context.view_layer.objects.active
        curve.name = name
        curve.data.bevel_depth = thickness
        curve.data.bevel_resolution = 2
        curve.color = color


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if not args:
        raise SystemExit(__doc__)
    folder = Path(args[0])
    manifest = json.loads((folder / "manifest.json").read_text())
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    scene = bpy.context.scene
    scene.name = "Medium crevice subgrid"
    import_lines(folder / "overlap_window.obj", "10x fruit-size window",
                 (0.2, 0.55, 1.0, 1.0), 0.13)
    import_lines(folder / "core_box.obj", "Ownership core",
                 (1.0, 0.7, 0.12, 1.0), 0.18)
    import_lines(folder / "graph_paths.obj", "Surface graph paths",
                 (0.1, 0.9, 0.35, 1.0), 0.09)
    boundary = folder / "graph_boundary_paths.obj"
    if boundary.stat().st_size:
        import_lines(boundary, "Paths leaving the overlap window",
                     (0.6, 0.35, 0.95, 1.0), 0.06)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1)
    prototype = bpy.context.object
    ball_mesh = prototype.data
    bpy.data.objects.remove(prototype, do_unlink=True)
    with (folder / "nodes.csv").open(newline="") as f:
        for row in csv.DictReader(f):
            obj = bpy.data.objects.new("sample_" + row["node_id"], ball_mesh)
            scene.collection.objects.link(obj)
            obj.location = (float(row["x"]), float(row["y"]), float(row["z"]))
            obj.scale = (0.65, 0.65, 0.65)
            obj.color = ((1.0, 0.25, 0.12, 1.0) if int(row["in_core"])
                         else (0.9, 0.55, 0.8, 1.0))
    low = Vector(manifest["window_low"])
    high = Vector(manifest["window_high"])
    center = (low + high) * 0.5
    span = max(high - low)
    bpy.ops.object.camera_add(location=center + Vector((0, -1.4 * span, 1.1 * span)))
    camera = bpy.context.object
    direction = center - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = span * 1.7
    scene.camera = camera
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = 1400
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.display.shading.color_type = "OBJECT"
    scene.display.shading.background_type = "WORLD"
    scene.world.color = (0.035, 0.035, 0.035)
    scene.render.filepath = str(folder / "medium_subgrid.png")
    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(folder / "medium_subgrid.blend"))


if __name__ == "__main__":
    main()
