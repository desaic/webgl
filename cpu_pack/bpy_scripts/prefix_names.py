import bpy

# Define the collections you want to process
target_collections = ["left", "center", "right"]

for col_name in target_collections:
    collection = bpy.data.collections.get(col_name)
    
    if not collection:
        print(f"Collection '{col_name}' not found. Skipping.")
        continue
        
    print(f"Processing collection: {col_name}")
    new_prefix = f"{col_name}_"
    
    # Loop through all objects directly in this collection
    for obj in collection.objects:
        current_name = obj.name
        
        if "_" in current_name:
            # Find the first underscore and split the string there
            # Example: "oldprefix_cube" -> "cube"
            _, base_name = current_name.split("_", 1)
            
            # If the split leaves an empty string (e.g. the object was just named "_"),
            # fallback to the original name to prevent losing it entirely.
            if not base_name:
                base_name = current_name
        else:
            # No underscore found, the whole name is the base name
            base_name = current_name
            
        # Assign the new prefixed name
        obj.name = f"{new_prefix}{base_name}"

print("Object renaming complete!")