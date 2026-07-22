import bpy
import mathutils

# Usage: set contact_file to your own contact_points.txt


def clean_part_name(part):
    """Processes a single object name based on your rules."""
    part = part.strip()
    if "___" in part:
        prefix = part[:4]
        # Get everything after the last '___'
        suffix = part.rpartition("___")[2]
        return f"{prefix}___{suffix}"
    else:
        return part[:4]

def process_pair_name(line_text):
    """Splits by '<->', processes both sides, and rejoins them."""
    if "<->" not in line_text:
        return line_text[:60]
    
    left_part, right_part = line_text.split("<->", 1)
    
    clean_left = clean_part_name(left_part)
    clean_right = clean_part_name(right_part)
    
    return f"{clean_left} <-> {clean_right}"

def import_contact_pairs(file_path, normal_length=0.03):
    with open(file_path, 'r') as f:
        lines = f.readlines()
    
    # 1. Create a single container collection
    collection_name = "Contact_Pairs"
    if collection_name in bpy.data.collections:
        contact_col = bpy.data.collections[collection_name]
    else:
        contact_col = bpy.data.collections.new(collection_name)
        bpy.context.scene.collection.children.link(contact_col)
    
    line_idx = 1  # Skip header ("Contact pairs: 480")
    total_lines = len(lines)
    
    while line_idx < total_lines:
        line = lines[line_idx].strip()
        
        # Look for the header line of a pair
        if not line or "<->" not in line:
            line_idx += 1
            continue
        
        # Safely truncate name for Blender's limit
        pair_name = process_pair_name(line)
        
        # Read number of points
        line_idx += 1
        num_points = int(lines[line_idx].strip())
        line_idx += 1
        
        verts = []
        edges = []
        
        # 2. Parse x y z and nx ny nz
        for _ in range(num_points):
            coords = list(map(float, lines[line_idx].split()))
            
            p_start = mathutils.Vector(coords[0:3])
            p_end = p_start + (mathutils.Vector(coords[3:6]) * normal_length)
            
            idx = len(verts)
            verts.extend([p_start, p_end])
            edges.append((idx, idx + 1))
            
            line_idx += 1
        
        # 3. Create single mesh object for pair
        mesh = bpy.data.meshes.new(name=pair_name)
        mesh.from_pydata(verts, edges, [])
        mesh.update()
        
        # Fixed: using object_data parameter
        obj = bpy.data.objects.new(name=pair_name, object_data=mesh)
        contact_col.objects.link(obj)

    print("Import completed successfully.")

contact_file = "/home/desaic/github/jarvis/packages/services/modules/modules_service/modules/parts_diagram_service/_data_folder/chats/429116fa-9c3d-4fe6-99c8-2be9320296c4/debug/contact_points.txt"
import_contact_pairs(contact_file,  normal_length=0.03)