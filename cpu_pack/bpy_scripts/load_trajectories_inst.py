import bpy
import math
import os
from mathutils import Matrix, Vector

def parse_transformation(line):
    tokens = line.split()
    if len(tokens) < 16:
        return None

    idx = tokens.index('pos')
    pos = Vector((float(tokens[idx+1]), float(tokens[idx+2]), float(tokens[idx+3])))

    idx = tokens.index('rot')
    rot_matrix = Matrix((
        (float(tokens[idx+1]), float(tokens[idx+2]), float(tokens[idx+3])),
        (float(tokens[idx+4]), float(tokens[idx+5]), float(tokens[idx+6])),
        (float(tokens[idx+7]), float(tokens[idx+8]), float(tokens[idx+9]))
    ))

    scale = 1.0
    if 'scale' in tokens:
        idx = tokens.index('scale')
        scale = float(tokens[idx+1])

    return pos, rot_matrix, scale

def load_trajectories(filepath):
    instances = []
    current_instance = None

    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                if current_instance:
                    instances.append(current_instance)
                    current_instance = None
                continue

            if line.startswith('Instance'):
                if current_instance:
                    instances.append(current_instance)

                tokens = line.split()
                instance_id = int(tokens[1])
                mesh_idx = tokens.index('Mesh')
                mesh_path = ' '.join(tokens[mesh_idx+1:])

                current_instance = {
                    'id': instance_id,
                    'mesh_path': mesh_path,
                    'final': None,
                    'trajectory': []
                }
                if current_instance["mesh_path"] == "":
                    current_instance["mesh_path"] = "cube";
                             
            elif line.startswith('Initial') or line.startswith('Final'):
                trans = parse_transformation(line)
                if trans and current_instance:
                    current_instance['final'] = trans

            elif line.startswith('Step'):
                trans = parse_transformation(line)
                if trans and current_instance:
                    current_instance['trajectory'].append(trans)

        if current_instance:
            instances.append(current_instance)

    return instances

def apply_transformation(obj, pos, rot_matrix, scale):
    obj.location = pos
    obj.rotation_mode = 'QUATERNION'
    obj.rotation_quaternion = rot_matrix.to_quaternion()
    obj.scale = (scale, scale, scale)

def get_or_create_cube_mesh():
    mesh_name = "Fallback_Cube_Mesh"
    if mesh_name in bpy.data.meshes:
        return bpy.data.meshes[mesh_name]

    # Create 1x1x1 cube (size=1.0 gives bounds from -0.5 to +0.5)
    bpy.ops.mesh.primitive_cube_add(size=1.0)
    cube_obj = bpy.context.active_object
    cube_mesh = cube_obj.data
    cube_mesh.name = mesh_name
    bpy.data.objects.remove(cube_obj, do_unlink=True)
    return cube_mesh

def get_or_create_items_collection(name="items"):
    """Return the collection `name` directly under the scene's root collection,
    creating and linking it first if it is not there."""
    root = bpy.context.scene.collection
    existing = root.children.get(name)
    if existing is not None:
        return existing
    # a collection of that name may exist in the file without being linked here.
    collection = bpy.data.collections.get(name)
    if collection is None:
        collection = bpy.data.collections.new(name)
    root.children.link(collection)
    return collection

def move_to_collection(obj, collection):
    for c in list(obj.users_collection):
        if c is not collection:
            c.objects.unlink(obj)
    if obj.name not in collection.objects:
        collection.objects.link(obj)

def animate_instances(instances, frames_per_step=3, pause_frames=5):
    current_frame = 1
    loaded_meshes = {}
    current_collection = get_or_create_items_collection()

    for inst in instances:
        mesh_path = inst['mesh_path']
        mesh_name = os.path.splitext(os.path.basename(mesh_path))[0] if mesh_path != "cube" else "cube"
        obj_name = f"{mesh_name}_{inst['id']}"

        if mesh_path not in loaded_meshes:
            mesh_data = None
            if mesh_path != "cube":
                try:
                    bpy.ops.wm.obj_import(filepath=mesh_path)
                    imported_obj = bpy.context.selected_objects[0]
                    move_to_collection(imported_obj, current_collection)
                    mesh_data = imported_obj.data
                    obj = imported_obj
                except Exception as e:
                    print(f"Failed to load '{mesh_path}': {e}. Using fallback 1x1x1 cube.")

            # Fallback if loading failed or path is 'cube'
            if mesh_data is None:
                mesh_data = get_or_create_cube_mesh()
                obj = bpy.data.objects.new(name=obj_name, object_data=mesh_data)
                current_collection.objects.link(obj)

            loaded_meshes[mesh_path] = mesh_data
        else:
            # Reuse already loaded/cached mesh block
            mesh_data = loaded_meshes[mesh_path]
            obj = bpy.data.objects.new(name=obj_name, object_data=mesh_data)
            current_collection.objects.link(obj)

        obj.name = obj_name

        # Apply initial state transforms
        if len(inst['trajectory']) > 0:
            pos, rot_matrix, scale = inst['trajectory'][0]
        else:
            pos, rot_matrix, scale = inst['final']
        apply_transformation(obj, pos, rot_matrix, scale)

        # Visibility Setup
        obj.hide_viewport = True
        obj.hide_render = True
        obj.keyframe_insert(data_path="hide_viewport", frame=max(1, current_frame - 1))
        obj.keyframe_insert(data_path="hide_render", frame=max(1, current_frame - 1))

        obj.hide_viewport = False
        obj.hide_render = False
        obj.keyframe_insert(data_path="hide_viewport", frame=current_frame)
        obj.keyframe_insert(data_path="hide_render", frame=current_frame)

        obj.keyframe_insert(data_path="location", frame=current_frame)
        obj.keyframe_insert(data_path="rotation_quaternion", frame=current_frame)
        obj.keyframe_insert(data_path="scale", frame=current_frame)

        current_frame += pause_frames

        # Trajectory Animation
        for step_idx, (pos, rot_matrix, scale) in enumerate(inst['trajectory']):
            apply_transformation(obj, pos, rot_matrix, scale)

            obj.keyframe_insert(data_path="location", frame=current_frame)
            obj.keyframe_insert(data_path="rotation_quaternion", frame=current_frame)
            obj.keyframe_insert(data_path="scale", frame=current_frame)

            current_frame += frames_per_step

        current_frame += pause_frames

    bpy.context.scene.frame_end = current_frame

def main():
    #trajectory_file = "/media/desaic/WD/meshes/fruit_hand/out/traj3.txt"
    #trajectory_file = "/media/desaic/WD/meshes/fruit_hand/traj3_0719.txt"
    trajectory_file = "F:/meshes/fruit_hand/out_hand_medium_8_14/traj8.txt"

    instances = load_trajectories(trajectory_file)
    print(f"Loaded {len(instances)} instances")

    animate_instances(instances, frames_per_step=1, pause_frames=2)

    print("Animation complete")

if __name__ == "__main__":
    main()