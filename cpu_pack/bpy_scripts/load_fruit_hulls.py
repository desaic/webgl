"""Lay out every fruit next to its convex decomposition in Blender.

In Blender's Text Editor, open this script and press Run Script. Each fruit from FRUIT_DIR is
imported and placed in a rectangular grid, and the mesh of the same name from HULL_DIR (one
object per convex hull, joined into one) is placed at exactly the same location, so the hulls
sit on top of the fruit. Hulls are drawn as wireframe.

From the command line:
  blender --python load_fruit_hulls.py -- [FRUIT_DIR HULL_DIR]
"""

import math
import os
import sys

import bpy
from mathutils import Vector

FRUIT_DIR = "F:/meshes/fruit_hand/fruits_1"
HULL_DIR = "F:/meshes/fruit_hand/fruit_coacd"
# gap between neighbouring grid cells, as a fraction of the largest mesh extent.
GAP = 0.25


def get_collection(name):
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(coll)
    return coll


def import_obj(path):
    """imports an obj, returns the new objects."""
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def move_to_collection(obj, coll):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    coll.objects.link(obj)


def join_objects(objs):
    """joins imported objects (one per hull) into the first one."""
    objs = [o for o in objs if o.type == 'MESH']
    if len(objs) <= 1:
        return objs[0] if objs else None
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    return objs[0]


def bbox_of(obj):
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = Vector((min(c[i] for c in corners) for i in range(3)))
    hi = Vector((max(c[i] for c in corners) for i in range(3)))
    return lo, hi


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    fruit_dir = args[0] if args else FRUIT_DIR
    hull_dir = args[1] if len(args) > 1 else HULL_DIR

    names = sorted(f[:-4] for f in os.listdir(fruit_dir)
                   if f.lower().endswith(".obj") and not f[:-4].endswith("_m"))
    names = [n for n in names if os.path.exists(os.path.join(hull_dir, n + ".obj"))]
    print(f"{len(names)} fruits with a hull mesh in {hull_dir}")

    fruit_coll = get_collection("fruits")
    hull_coll = get_collection("coacd_hulls")

    pairs = []
    for name in names:
        fruit_objs = import_obj(os.path.join(fruit_dir, name + ".obj"))
        fruit = join_objects(fruit_objs)
        hull_objs = import_obj(os.path.join(hull_dir, name + ".obj"))
        hull = join_objects(hull_objs)
        if fruit is None or hull is None:
            print(f"skipping {name}: nothing imported")
            continue
        fruit.name = name
        hull.name = name + "_coacd"
        move_to_collection(fruit, fruit_coll)
        move_to_collection(hull, hull_coll)
        hull.display_type = 'WIRE'
        hull.show_in_front = True
        pairs.append((fruit, hull))

    if not pairs:
        return

    # both meshes keep the source coordinates, so one offset moves the pair together. each pair
    # is centered on its grid cell by the fruit's bounding box.
    extents = []
    for fruit, hull in pairs:
        lo, hi = bbox_of(fruit)
        extents.append(max(hi - lo))
    cell = max(extents) * (1.0 + GAP)
    cols = math.ceil(math.sqrt(len(pairs)))
    for i, (fruit, hull) in enumerate(pairs):
        lo, hi = bbox_of(fruit)
        center = 0.5 * (lo + hi)
        target = Vector(((i % cols) * cell, -(i // cols) * cell, 0.0))
        offset = target - center
        fruit.location += offset
        hull.location += offset
    print(f"placed {len(pairs)} fruit/hull pairs in a {cols} column grid, cell {cell:.1f}")


if __name__ == "__main__":
    main()
