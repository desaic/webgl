"""Render the local fruit pack and crevice-face OBJ from several views.

blender --background --python inspect_crevice_faces.py -- PACK MESH_DIR FACES OUTPUT_DIR
"""
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    pack_path, mesh_dir, faces_path, output = (Path(a) for a in args[:4])
    output.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    center = Vector((-231, -39, -46))
    cache = {}
    fruit_objects = []
    for i, line in enumerate(pack_path.read_text().splitlines()[1:]):
        t = line.split()
        name = t[0]
        if name not in cache:
            bpy.ops.object.select_all(action="DESELECT")
            bpy.ops.wm.obj_import(filepath=str(mesh_dir / (name + ".obj")),
                                  forward_axis="Y", up_axis="Z")
            source = bpy.context.selected_objects[0]
            cache[name] = source.data
            bpy.data.objects.remove(source, do_unlink=True)
        obj = bpy.data.objects.new(f"fruit_{i:02d}_{name}", cache[name])
        bpy.context.scene.collection.objects.link(obj)
        j = t.index("pos")
        obj.location = tuple(map(float, t[j + 1:j + 4]))
        j = t.index("rot")
        rot = list(map(float, t[j + 1:j + 10]))
        obj.rotation_mode = "QUATERNION"
        obj.rotation_quaternion = Matrix((rot[:3], rot[3:6], rot[6:9])).to_quaternion()
        obj.scale = (float(t[t.index("scale") + 1]),) * 3
        obj.color = (0.7, 0.72, 0.77, 1)
        fruit_objects.append(obj)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.wm.obj_import(filepath=str(faces_path), forward_axis="Y", up_axis="Z")
    face_objects = list(bpy.context.selected_objects)
    for obj in face_objects:
        obj.name = "crevice_faces"
        obj.color = (1.0, 0.42, 0.06, 1)
    if len(args) > 4:
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=0.35)
        marker_mesh = bpy.context.object.data
        bpy.data.objects.remove(bpy.context.object, do_unlink=True)
        for i, line in enumerate(Path(args[4]).read_text().splitlines()):
            if not line.startswith("v "):
                continue
            marker = bpy.data.objects.new(f"uncovered_{i}", marker_mesh)
            bpy.context.scene.collection.objects.link(marker)
            marker.location = tuple(map(float, line.split()[1:4]))
            marker.color = (0.1, 0.75, 1.0, 1)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = 1500
    scene.render.resolution_y = 1050
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.display.shading.color_type = "OBJECT"
    scene.display.shading.show_cavity = True
    scene.display.shading.background_type = "WORLD"
    scene.world.color = (0.03, 0.03, 0.03)
    bpy.ops.object.camera_add()
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 84
    scene.camera = camera
    for view, offset in {
        "front": (0, -110, 32),
        "back": (0, 110, 32),
        "top": (0, -25, 110),
    }.items():
        camera.location = center + Vector(offset)
        camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
        for obj in fruit_objects:
            obj.hide_render = False
        scene.render.filepath = str(output / f"{view}_fruit_faces.png")
        bpy.ops.render.render(write_still=True)
        for obj in fruit_objects:
            obj.hide_render = True
        scene.render.filepath = str(output / f"{view}_faces_only.png")
        bpy.ops.render.render(write_still=True)
    for obj in fruit_objects:
        obj.hide_render = False
    bpy.ops.wm.save_as_mainfile(filepath=str(output / "crevice_faces_x208.blend"))
    print("Wrote diagnostic scene to", output)


if __name__ == "__main__":
    main()
