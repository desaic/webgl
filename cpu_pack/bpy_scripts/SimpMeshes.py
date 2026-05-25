import bpy

def simplify_mesh_collection(collection_name):
    # Grab the target collection
    collection = bpy.data.collections.get(collection_name)
    if not collection:
        print(f"Error: Collection '{collection_name}' not found.")
        return

    view_layer = bpy.context.view_layer

    # Force Object Mode to apply modifiers destructively
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    # Build a stable list of target meshes
    mesh_objects = [obj for obj in collection.objects if obj.type == 'MESH']
    
    print(f"Starting simplification pipeline for {len(mesh_objects)} meshes...")

    for obj in mesh_objects:
        # Isolate context selection to the active mesh
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        view_layer.objects.active = obj

        # 1. Merge Close Vertices (Distance < 0.01)
        weld_mod = obj.modifiers.new(name="TmpWeld", type='WELD')
        weld_mod.merge_threshold = 0.01
        bpy.ops.object.modifier_apply(modifier=weld_mod.name)

        # 2. Evaluate Triangle Limits & Decimate by 0.5
        obj.data.calc_loop_triangles()
        tri_count = len(obj.data.loop_triangles)
        
        if tri_count > 90000:
            print(f"-> '{obj.name}' has {tri_count} triangles. Decimating by 0.5...")
            dec_mod = obj.modifiers.new(name="TmpDecimate", type='DECIMATE')
            dec_mod.ratio = 0.25
            bpy.ops.object.modifier_apply(modifier=dec_mod.name)
        elif tri_count > 20000:
            print(f"-> '{obj.name}' has {tri_count} triangles. Decimating by 0.5...")
            dec_mod = obj.modifiers.new(name="TmpDecimate", type='DECIMATE')
            dec_mod.ratio = 0.5
            bpy.ops.object.modifier_apply(modifier=dec_mod.name)

    print("Simplification complete!")

# Execute Simplification
simplify_mesh_collection("fruit")