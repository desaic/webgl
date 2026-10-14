"""Render and save fruit, SDF-derived passage centers, and sphere cover.

blender --background --python inspect_crevice_mouths.py -- PACK MESH_DIR OUTPUT_DIR SCENE_DIR
"""
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


def vertices(path):
    return [tuple(map(float, line.split()[1:4])) for line in path.read_text().splitlines()
            if line.startswith("v ")]


def main():
    pack, mesh_dir, output, destination = map(Path, sys.argv[sys.argv.index("--") + 1:][:4])
    destination.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    scene = bpy.context.scene
    collections = {}
    for name in ("Fruit", "All gap centers", "Covered medium centers",
                 "Uncovered medium centers", "Placed spheres"):
        collection = bpy.data.collections.new(name)
        scene.collection.children.link(collection)
        collections[name] = collection

    cache = {}
    for i, line in enumerate(pack.read_text().splitlines()[1:]):
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
        collections["Fruit"].objects.link(obj)
        obj.location = tuple(map(float, t[t.index("pos") + 1:t.index("pos") + 4]))
        rot = list(map(float, t[t.index("rot") + 1:t.index("rot") + 10]))
        obj.rotation_mode = "QUATERNION"
        obj.rotation_quaternion = Matrix((rot[:3], rot[3:6], rot[6:9])).to_quaternion()
        obj.scale = (float(t[t.index("scale") + 1]),) * 3
        obj.color = (0.72, 0.74, 0.77, 1)

    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1)
    marker_mesh = bpy.context.object.data
    bpy.data.objects.remove(bpy.context.object, do_unlink=True)
    for path, collection, radius, color in (
        (output / "crevice_mouth_points.obj", "All gap centers", 0.23, (0.04, 0.65, 0.95, 1)),
        (output / "crevice_mouth_medium_covered.obj", "Covered medium centers", 0.38,
         (0.95, 0.7, 0.05, 1)),
        (output / "crevice_mouth_medium_uncovered.obj", "Uncovered medium centers", 0.43,
         (1.0, 0.07, 0.04, 1)),
    ):
        for i, p in enumerate(vertices(path)):
            obj = bpy.data.objects.new(f"{collection}_{i:04d}", marker_mesh)
            collections[collection].objects.link(obj)
            obj.location = p
            obj.scale = (radius,) * 3
            obj.color = color

    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1)
    sphere_mesh = bpy.context.object.data
    bpy.data.objects.remove(bpy.context.object, do_unlink=True)
    for i, line in enumerate((output / "sphere_cover.txt").read_text().splitlines()[1:]):
        x, y, z, radius = map(float, line.split()[:4])
        obj = bpy.data.objects.new(f"sphere_{i:03d}", sphere_mesh)
        collections["Placed spheres"].objects.link(obj)
        obj.location = (x, y, z)
        obj.scale = (radius,) * 3
        obj.color = (0.95, 0.18, 0.55, 1)
        wire = obj.modifiers.new("wire", "WIREFRAME")
        wire.thickness = 0.04
        wire.use_replace = True

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
    center = Vector((-231, -39, -46))
    for label, offset in {"front": (0, -110, 32), "back": (0, 110, 32)}.items():
        camera.location = center + Vector(offset)
        camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
        for name in ("All gap centers", "Covered medium centers",
                     "Uncovered medium centers", "Placed spheres"):
            collections[name].hide_render = name != "All gap centers"
        scene.render.filepath = str(destination / f"{label}_all_gaps.png")
        bpy.ops.render.render(write_still=True)
        collections["All gap centers"].hide_render = True
        collections["Covered medium centers"].hide_render = False
        collections["Uncovered medium centers"].hide_render = False
        scene.render.filepath = str(destination / f"{label}_medium_width.png")
        bpy.ops.render.render(write_still=True)
        collections["Placed spheres"].hide_render = False
        scene.render.filepath = str(destination / f"{label}_medium_with_spheres.png")
        bpy.ops.render.render(write_still=True)
    collections["All gap centers"].hide_render = False
    collections["Placed spheres"].hide_render = True
    bpy.ops.wm.save_as_mainfile(filepath=str(destination / "crevice_mouths_x208.blend"))


if __name__ == "__main__":
    main()
