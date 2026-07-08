import bpy
import os
from mathutils import Matrix, Vector

# Export every visible mesh object in the current scene to its own .obj
# file (geometry centered at bounding box center) and write a pack.txt whose 
# lines are compatible with PackingScene::LoadPack in PackingScene.cpp:
#
#   ItemName pos x y z rot r00 r01 r02 r10 r11 r12 r20 r21 r22 scale s
#
# Each mesh is exported centered around its bounding box center, and its 
# world transform is adjusted and recorded in pack.txt.

PACK_FILENAME = "pack.txt"


def format_float(x):
    return f"{x:.6f}"


def matrix3_to_row_major_strings(m):
    # m is a 3x3 mathutils Matrix. Output row-major: r00 r01 r02 r10 ...
    vals = []
    for row in range(3):
        for col in range(3):
            vals.append(format_float(m[row][col]))
    return vals


def get_local_bbox_center(obj):
    # Calculate the center of the bounding box in local space
    local_bbox_coords = [Vector(corner) for corner in obj.bound_box]
    bbox_center = sum(local_bbox_coords, Vector((0.0, 0.0, 0.0))) / 8.0
    return bbox_center


def export_centered_object_local_space(obj, filepath):
    # Cache original world matrix and vertex positions
    orig_matrix = obj.matrix_world.copy()
    mesh = obj.data
    
    # Calculate how much we need to shift vertices to center the bounding box at (0,0,0)
    local_center = get_local_bbox_center(obj)
    
    # Shift vertices locally so the geometry bounding box is centered at the origin
    for vertex in mesh.vertices:
        vertex.co -= local_center
    mesh.update()

    try:
        # Reset object origin to identity for local-space export
        obj.matrix_world = Matrix.Identity(4)
        bpy.context.view_layer.update()
        
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        
        bpy.ops.wm.obj_export(
            filepath=filepath,
            export_selected_objects=True,
            export_materials=False,
            export_uv=False,
            export_normals=False,
            export_triangulated_mesh=True,
        )
    finally:
        # Restore original vertex positions
        for vertex in mesh.vertices:
            vertex.co += local_center
        mesh.update()
        
        # Restore original world matrix
        obj.matrix_world = orig_matrix
        bpy.context.view_layer.update()
        
    return local_center


def build_pack_line(name, world_matrix, local_center):
    # Because we shifted the geometry by -local_center, the new local origin
    # of the mesh is effectively at +local_center. We must factor this shift
    # into the world position so the external loader places it correctly.
    adjusted_world_matrix = world_matrix @ Matrix.Translation(local_center)
    
    loc, rot_quat, scale = adjusted_world_matrix.decompose()
    rot_mat = rot_quat.to_matrix()
    
    # Use the uniform scale component
    s = scale[0]
    parts = [name, "pos",
             format_float(loc.x), format_float(loc.y), format_float(loc.z),
             "rot"] + matrix3_to_row_major_strings(rot_mat) + \
            ["scale", format_float(s)]
    return " ".join(parts)


def main():
    blend_filepath = bpy.data.filepath
    if not blend_filepath:
        print("Warning: the .blend file has not been saved. "
              "Save it first so exports land next to it.")
        return
    blend_dir = os.path.dirname(os.path.abspath(blend_filepath))
    os.chdir(blend_dir)
    print(f"Working directory: {blend_dir}")

    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    view_layer = bpy.context.view_layer
    original_active = view_layer.objects.active
    original_selection = list(bpy.context.selected_objects)

    mesh_objects = [obj for obj in bpy.data.objects
                    if obj.type == 'MESH' and obj.visible_get()]

    pack_lines = []
    exported_count = 0

    print(f"Exporting {len(mesh_objects)} visible mesh object(s)...")

    for obj in mesh_objects:
        safe_name = bpy.path.clean_name(obj.name)
        if not safe_name:
            print(f"Skipping object with empty name: {obj.name}")
            continue

        # Record the world transform before modifying anything
        world_matrix = obj.matrix_world.copy()

        obj_filepath = os.path.join(blend_dir, safe_name + ".obj")
        
        # Export and get the local center offset used for centering
        local_center = export_centered_object_local_space(obj, obj_filepath)
        
        print(f"Exported: {safe_name}.obj (Centered)")
        exported_count += 1

        # Build the line using the adjusted matrix
        pack_lines.append(build_pack_line(safe_name, world_matrix, local_center))

    pack_path = os.path.join(blend_dir, PACK_FILENAME)
    with open(pack_path, "w") as f:
        f.write(f"{len(pack_lines)}\n")
        for line in pack_lines:
            f.write(line + "\n")
    print(f"Wrote {pack_path} ({len(pack_lines)} instance line(s))")

    # Restore viewport selection state.
    bpy.ops.object.select_all(action='DESELECT')
    for obj in original_selection:
        try:
            obj.select_set(True)
        except ReferenceError:
            pass
    view_layer.objects.active = original_active

    print(f"Done. Exported {exported_count} mesh(es) to {blend_dir}")


main()