"""Render a close view of a pack around the strawberry2 opening.

blender --background --python render_medium_hole.py -- PACK MESH_DIR OUTPUT_PREFIX
"""
import sys
import csv
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    pack_path, mesh_dir, out = Path(args[0]), Path(args[1]), Path(args[2])
    targets_path = Path(args[3]) if len(args) > 3 else None
    center = Vector((-168.154, -60.4725, -48.5998))
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    cache = {}
    count = 0
    for line in pack_path.read_text().splitlines()[1:]:
        t = line.split()
        if len(t) < 16:
            continue
        pos = Vector(tuple(map(float, t[t.index("pos") + 1:t.index("pos") + 4])))
        if (pos - center).length > 27:
            continue
        name = t[0]
        path = mesh_dir / (name + ".obj")
        if not path.exists():
            continue
        if name not in cache:
            bpy.ops.object.select_all(action="DESELECT")
            bpy.ops.wm.obj_import(filepath=str(path), forward_axis="Y", up_axis="Z")
            source = bpy.context.selected_objects[0]
            cache[name] = source.data
            bpy.data.objects.remove(source, do_unlink=True)
        obj = bpy.data.objects.new(f"{name}_{count}", cache[name])
        bpy.context.scene.collection.objects.link(obj)
        obj.location = pos
        r = list(map(float, t[t.index("rot") + 1:t.index("rot") + 10]))
        obj.rotation_mode = "QUATERNION"
        obj.rotation_quaternion = Matrix((r[:3], r[3:6], r[6:9])).to_quaternion()
        obj.color = ((1.0, 0.2, 0.1, 1.0) if (pos - center).length < 0.01
                     else (0.7, 0.72, 0.76, 1.0))
        count += 1
    if targets_path:
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=0.42)
        marker_mesh = bpy.context.object.data
        bpy.data.objects.remove(bpy.context.object, do_unlink=True)
        with targets_path.open(newline="") as file:
            for index, row in enumerate(csv.DictReader(file)):
                point = Vector((float(row["x"]), float(row["y"]), float(row["z"])))
                if (point - center).length > 16 or row["initially_covered"] != "0":
                    continue
                marker = bpy.data.objects.new(f"target_{index}", marker_mesh)
                scene = bpy.context.scene
                scene.collection.objects.link(marker)
                marker.location = point
                marker.color = ((0.15, 1.0, 0.25, 1.0) if row["wrap_radius_cm"] == "6"
                                else (1.0, 0.9, 0.1, 1.0))
    scene = bpy.context.scene
    bpy.ops.object.camera_add(location=center + Vector((0, -80, 28)))
    camera = bpy.context.object
    camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 45
    scene.camera = camera
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = 1400
    scene.render.resolution_y = 1100
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.display.shading.color_type = "OBJECT"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.show_cavity = True
    scene.display.shading.background_type = "WORLD"
    scene.world.color = (0.04, 0.04, 0.04)
    scene.render.filepath = str(out.with_suffix(".png"))
    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(out.with_suffix(".blend")))
    print("Rendered", count, "fruit objects to", scene.render.filepath)


if __name__ == "__main__":
    main()
