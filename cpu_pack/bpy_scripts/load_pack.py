import bpy
import os
from mathutils import Matrix, Vector

PACK_FILE = "/media/desaic/WD/meshes/fruit_hand/pack_debug_10.txt"
MESH_DIR = "/media/desaic/WD/meshes/fruit_hand/fruits_1/"


def parse_transformation(tokens):
    idx = tokens.index('pos')
    pos = Vector((float(tokens[idx + 1]), float(tokens[idx + 2]),
                  float(tokens[idx + 3])))

    idx = tokens.index('rot')
    rot_matrix = Matrix((
        (float(tokens[idx + 1]), float(tokens[idx + 2]), float(tokens[idx + 3])),
        (float(tokens[idx + 4]), float(tokens[idx + 5]), float(tokens[idx + 6])),
        (float(tokens[idx + 7]), float(tokens[idx + 8]), float(tokens[idx + 9]))
    ))

    scale = 1.0
    if 'scale' in tokens:
        idx = tokens.index('scale')
        scale = float(tokens[idx + 1])

    return pos, rot_matrix, scale


def load_pack(filepath):
    instances = []
    with open(filepath, 'r') as f:
        lines = f.readlines()

    if not lines:
        return instances

    # First line is the instance count.
    try:
        expected_count = int(lines[0].strip())
    except ValueError:
        expected_count = 0
        lines = [lines[0]] + lines

    for line in lines[1:]:
        line = line.strip()
        if not line:
            continue
        tokens = line.split()
        if len(tokens) < 16:
            print(f"Skipping malformed line: {line}")
            continue

        name = tokens[0]
        trans = parse_transformation(tokens)
        if trans is None:
            continue
        instances.append({'name': name, 'transform': trans})

    if expected_count and len(instances) != expected_count:
        print(f"Warning: expected {expected_count} instances, "
              f"parsed {len(instances)}")

    return instances


def apply_transformation(obj, pos, rot_matrix, scale):
    obj.location = pos
    obj.rotation_mode = 'QUATERNION'
    obj.rotation_quaternion = rot_matrix.to_quaternion()
    obj.scale = (scale, scale, scale)


def resolve_mesh_dir(mesh_dir):
    if mesh_dir.startswith("//"):
        return bpy.path.abspath(mesh_dir)
    return os.path.abspath(mesh_dir)


def instantiate(instances, mesh_dir, collection):
    loaded_meshes = {}
    mesh_dir = resolve_mesh_dir(mesh_dir)

    for i, inst in enumerate(instances):
        name = inst['name']
        mesh_path = os.path.join(mesh_dir, name + ".obj")

        if not os.path.exists(mesh_path):
            print(f"Missing mesh file: {mesh_path}")
            continue

        if mesh_path not in loaded_meshes:
            bpy.ops.wm.obj_import(filepath=mesh_path)
            obj = bpy.context.selected_objects[0]
            loaded_meshes[mesh_path] = obj.data
        else:
            mesh_data = loaded_meshes[mesh_path]
            obj = bpy.data.objects.new(name=f"{name}_{i}", object_data=mesh_data)
            collection.objects.link(obj)

        obj.name = f"{name}_{i}"
        obj.data.name = name

        pos, rot_matrix, scale = inst['transform']
        apply_transformation(obj, pos, rot_matrix, scale)

    return len(instances)


def main():
    pack_path = PACK_FILE
    if pack_path.startswith("//"):
        pack_path = bpy.path.abspath(pack_path)

    instances = load_pack(pack_path)
    print(f"Loaded {len(instances)} instances from {pack_path}")

    collection = bpy.context.collection
    count = instantiate(instances, MESH_DIR, collection)
    print(f"Instantiated {count} objects from {MESH_DIR}")


if __name__ == "__main__":
    main()
