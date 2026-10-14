"""Load a cpu_pack sphere_cover.txt in Blender as sphere instances.

File format: first line is the sphere count, then one "x y z r" per line.
In Blender's Text Editor press Run Script, or run:
  blender --python load_spheres.py -- SPHERE_FILE
All spheres share one low detail ico sphere mesh (unit radius) and are
scaled to their radius.
"""

import bpy
import bmesh
import sys

SPHERE_FILE = "F:/meshes/fruit_hand/out_hand_shellpass_x208/shell_pass1_spheres.txt"
SUBDIVISIONS = 2


def load_spheres(filepath):
    spheres = []
    with open(filepath, 'r') as f:
        lines = f.readlines()
    for line in lines[1:]:
        tokens = line.split()
        if len(tokens) < 4:
            continue
        x, y, z, r = (float(t) for t in tokens[:4])
        spheres.append((x, y, z, r))
    return spheres


def make_unit_ico(name):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=SUBDIVISIONS, radius=1.0)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    path = args[0] if args else SPHERE_FILE
    if path.startswith("//"):
        path = bpy.path.abspath(path)

    spheres = load_spheres(path)
    print(f"Loaded {len(spheres)} spheres from {path}")

    mesh = make_unit_ico("sphere_proxy")
    collection = bpy.data.collections.new("sphere_cover")
    bpy.context.scene.collection.children.link(collection)
    for i, (x, y, z, r) in enumerate(spheres):
        obj = bpy.data.objects.new(f"sphere_{i}", mesh)
        obj.location = (x, y, z)
        obj.scale = (r, r, r)
        collection.objects.link(obj)
    print(f"Created {len(spheres)} sphere instances")


if __name__ == "__main__":
    main()
