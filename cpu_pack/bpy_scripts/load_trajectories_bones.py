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

def animate_instances(instances, frames_per_step=3, pause_frames=5):
    current_frame = 1
    
    # Dictionary to cache mesh data blocks: { mesh_path : bpy.types.Mesh }
    loaded_meshes = {}
    current_collection = bpy.context.collection

    for inst in instances:
        mesh_path = inst['mesh_path']

        if mesh_path not in loaded_meshes:
            # 1. First time seeing this mesh: Load from disk
            bpy.ops.wm.obj_import(filepath=mesh_path)
            obj = bpy.context.selected_objects[0]
            # Cache the underlying mesh block reference
            loaded_meshes[mesh_path] = obj.data
        else:
            # 2. Mesh already exists: Create an instant data link copy
            mesh_data = loaded_meshes[mesh_path]
            mesh_name = os.path.splitext(os.path.basename(mesh_path))[0]
            obj = bpy.data.objects.new(name=f"{mesh_name}_{inst['id']}", object_data=mesh_data)
            current_collection.objects.link(obj)

        # Ensure the outer object wrapper has the unique instance ID name
        mesh_name = os.path.splitext(os.path.basename(mesh_path))[0]
        obj.name = f"{mesh_name}_{inst['id']}"

        # Apply initial state transforms
        if len(inst['trajectory']) > 0:
            pos, rot_matrix, scale = inst['trajectory'][0]
        else :
            pos, rot_matrix, scale = inst['final']
        apply_transformation(obj, pos, rot_matrix, scale)

        # Visibility Keyframing Setup
        obj.hide_viewport = True
        obj.hide_render = True
        obj.keyframe_insert(data_path="hide_viewport", frame=current_frame-1)
        obj.keyframe_insert(data_path="hide_render", frame=current_frame-1)

        obj.hide_viewport = False
        obj.hide_render = False
        obj.keyframe_insert(data_path="hide_viewport", frame=current_frame)
        obj.keyframe_insert(data_path="hide_render", frame=current_frame)

        obj.keyframe_insert(data_path="location", frame=current_frame)
        obj.keyframe_insert(data_path="rotation_quaternion", frame=current_frame)
        obj.keyframe_insert(data_path="scale", frame=current_frame)

        current_frame += pause_frames

        # Step-by-Step Trajectory Animation
        for step_idx, (pos, rot_matrix, scale) in enumerate(inst['trajectory']):
            apply_transformation(obj, pos, rot_matrix, scale)

            obj.keyframe_insert(data_path="location", frame=current_frame)
            obj.keyframe_insert(data_path="rotation_quaternion", frame=current_frame)
            obj.keyframe_insert(data_path="scale", frame=current_frame)

            current_frame += frames_per_step

        current_frame += pause_frames

    bpy.context.scene.frame_end = current_frame

def main():
    trajectory_file = "/media/desaic/WD/meshes/skeleton/out/traj2.txt"

    instances = load_trajectories(trajectory_file)
    print(f"Loaded {len(instances)} instances")

    animate_instances(instances, frames_per_step=1, pause_frames=2)

    print("Animation complete")

if __name__ == "__main__":
    main()