import os
import bpy
import mathutils
from mathutils import Euler
from math import radians

def load_mesh_positions(filepath: str) -> list[dict]:
    parsed_data = []

    # Check if the file exists before attempting to open
    if not os.path.exists(filepath):
        print(f"Error: File not found at '{filepath}'")
        return []

    try:
        # Using 'with open' ensures the file is properly closed
        with open(filepath, 'r') as f:
            line_iterator = iter(f)        
            while True:
                try:
                    name_line = next(line_iterator).strip()
                    # Skip empty lines that might interrupt the pair sequence
                    if not name_line:
                        continue
                    
                    # 2. Read the Position line
                    # If the file ends here, StopIteration will be caught below
                    position_line = next(line_iterator).strip()
                    position = [float(p) for p in position_line.split() if p]
                    position[2] = 0
                    rotation_line = next(line_iterator).strip()
                    rotation = [float(p) for p in rotation_line.split() if p]
                    # Store the structured data
                    parsed_data.append({
                        'name': name_line,
                        'position': position,
                        'rotation': rotation
                    })

                except StopIteration:
                    break
                
                except ValueError:
                    print(f"Warning: Skipping data for '{name_line}'. Position line was malformed (expected numbers).")
                    continue

    except IOError as e:
        print(f"An I/O error occurred: {e}")
        return []

    return parsed_data

def set_pos_rot(name: str, pos, rot):
    if name in bpy.data.objects:
        obj = bpy.data.objects[name]
        obj.location = pos
        x_rad = radians(rot[0])
        y_rad = radians(rot[1])
        z_rad = radians(rot[2])
        euler = Euler((x_rad, y_rad, z_rad), 'ZYX')
        rot_mat = euler.to_matrix()
        obj.rotation_euler = rot_mat.to_euler('XYZ')
    else:
        print(f"Error: Object '{name}' not found.")
        return

parsed_data = load_mesh_positions('F:/meshes/skeleton/center_pos.txt')
for item in parsed_data:
    print(f"Name: {item['name']}")
    print(f"Position: {item['position']}")
    print(f"Rotation: {item['rotation']}\n")
    set_pos_rot(item['name'], item['position'], item['rotation'])