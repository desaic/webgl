import bpy
import os

def cleanup_and_export_vertices_faces(collection_name, output_dir):
    # 1. Grab the target collection
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

    # Force Object Mode to apply modifiers destructively
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    # Build a stable list of target meshes
    mesh_objects = [obj for obj in collection.objects if obj.type == 'MESH']
    exported_count = 0

    print(f"Beginning raw export pipeline for {len(mesh_objects)} meshes...")

    for obj in mesh_objects:
        # Isolate context selection to the active mesh
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        view_layer.objects.active = obj

        # --- STEP 1: Merge Close Vertices (Distance < 0.01) ---
        weld_mod = obj.modifiers.new(name="TmpWeld", type='WELD')
        weld_mod.merge_threshold = 0.01
        bpy.ops.object.modifier_apply(modifier=weld_mod.name)

        # --- STEP 2: Evaluate Triangle Limits & Decimate ---
        obj.data.calc_loop_triangles()
        tri_count = len(obj.data.loop_triangles)
        
        if tri_count > 90000:
            print(f"-> '{obj.name}' has {tri_count} triangles. Decimating by 0.5...")
            dec_mod = obj.modifiers.new(name="TmpDecimate", type='DECIMATE')
            dec_mod.ratio = 0.5
            bpy.ops.object.modifier_apply(modifier=dec_mod.name)

        # --- STEP 3: Pure Geometry Export (No Metadata) ---
        safe_filename = bpy.path.clean_name(obj.name) + ".obj"
        filepath = os.path.join(abs_output_dir, safe_filename)
        
        # Explicit configurations for pure structural exports in Blender 5.0
        bpy.ops.wm.obj_export(
            filepath=filepath,
            export_selected_objects=True,
            export_materials=False,          # Skips generating .mtl files and matching assignment text
            export_uv=False,                 # Drops all 'vt' texture coordinates 
            export_normals=False,            # Drops all 'vn' structural vector rows
            export_triangulated_mesh=False   # Keeps original geometry topology layout
        )
        
        print(f"Saved pure geometry: {safe_filename}")
        exported_count += 1

    # Cleanup interface state
    bpy.ops.object.select_all(action='DESELECT')
    for obj in original_selection:
        try:
            obj.select_set(True)
        except ReferenceError:
            pass
            
    view_layer.objects.active = original_active
    print(f"Task finished! {exported_count} meshes saved with raw spatial tokens.")

# --- Run ---
cleanup_and_export_vertices_faces("fruit", "./fruits_new/")