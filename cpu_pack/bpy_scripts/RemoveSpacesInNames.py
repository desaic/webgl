import bpy

def remove_spaces_in_collection(collection_name):
    # Safely get the collection
    collection = bpy.data.collections.get(collection_name)
    
    if not collection:
        print(f"Collection '{collection_name}' not found.")
        return

    print(f"Processing collection: {collection_name}...")
    renamed_objects = 0
    renamed_meshes = 0

    for obj in collection.objects:
        # 1. Clean the Object wrapper name
        if " " in obj.name:
            old_name = obj.name
            obj.name = obj.name.replace(" ", "")
            renamed_objects += 1
            print(f"Object: '{old_name}' -> '{obj.name}'")
            
        # 2. Clean the underlying Mesh block name (if it's a mesh)
        if obj.type == 'MESH' and obj.data:
            if " " in obj.data.name:
                old_data_name = obj.data.name
                obj.data.name = obj.data.name.replace(" ", "")
                renamed_meshes += 1
                print(f"  Mesh Data: '{old_data_name}' -> '{obj.data.name}'")

    print(f"Done! Cleaned {renamed_objects} objects and {renamed_meshes} mesh blocks.")

# Run the function for your "fruit" collection
remove_spaces_in_collection("fruit")