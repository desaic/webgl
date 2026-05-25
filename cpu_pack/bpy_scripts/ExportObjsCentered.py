import bpy
import os

def export_centered_raw_geometry(collection_name, output_dir):
    # Grab the target collection
    collection = bpy.data.collections.get(collection_name)
    if not collection:
        print(f"Error: Collection '{collection_name}' not found.")
        return

    # Resolve output directory paths safely
    if output_dir.startswith("//"):
        abs_output_dir = bpy.path.abspath(output_dir)
    else:
        abs_output_dir = os.path.abspath(output_dir)
    os.makedirs(abs_output_dir, exist_ok=True)

    view_layer = bpy.context.view_layer
    original_active = view_layer.objects.active
    original_selection = list(bpy.context.selected_objects)

    # Force Object Mode
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    mesh_objects = [obj for obj in collection.objects if obj.type == 'MESH']
    exported_count = 0

    print(f"Beginning centered export pipeline for {len(mesh_objects)} meshes...")

    for obj in mesh_objects:
        # Isolate context selection to the active mesh
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        view_layer.objects.active = obj

        # 1. Snap object's origin pivot directly to its bounding box center
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='BOUNDS')

        # 2. Cache original world position, then teleport to (0,0,0)
        # This guarantees that the exported OBJ file coordinates are perfectly centered around 0,0,0
        original_location = obj.location.copy()
        obj.location = (0.0, 0.0, 0.0)

        # 3. Pure Geometry Export (No Normals, No UVs, No Materials)
        safe_filename = bpy.path.clean_name(obj.name) + ".obj"
        filepath = os.path.join(abs_output_dir, safe_filename)
        
        bpy.ops.wm.obj_export(
            filepath=filepath,
            export_selected_objects=True,
            export_materials=False,          # Omit .mtl files
            export_uv=False,                 # Omit 'vt' texture coordinate lines
            export_normals=False,            # Omit 'vn' normal vectors
            export_triangulated_mesh=True   # Keep original face layouts
        )
        
        # 4. Restore the object back to its original location in your layout
        obj.location = original_location
        
        print(f"Exported centered geometry: {safe_filename}")
        exported_count += 1

    # Restore initial user viewport states
    bpy.ops.object.select_all(action='DESELECT')
    for obj in original_selection:
        try:
            obj.select_set(True)
        except ReferenceError:
            pass
            
    view_layer.objects.active = original_active
    print(f"Task finished! {exported_count} centered meshes saved to {abs_output_dir}")

# 1. Get the raw file path of the current open .blend file
blend_filepath = bpy.data.filepath

if blend_filepath:
    # 2. Extract the absolute directory path containing the file
    blend_dir = os.path.dirname(os.path.abspath(blend_filepath))
    
    # 3. Change the operating system's current working directory
    os.chdir(blend_dir)
    
    print(f"OS Current Working Directory successfully updated to:\n-> {os.getcwd()}")
else:
    # Fallback to prevent os.chdir erroring out on ""
    print("Warning: The current file has not been saved yet! Please save your .blend file first.")
# Execute Exporting
export_centered_raw_geometry("fruit", "./fruits_new/")