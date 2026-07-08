import bpy
from mathutils import Euler, Matrix

def print_matrix_3x3(matrix):
    """Prints the 3x3 portion of a mathutils.Matrix."""
    print("--- 3x3 Matrix Values ---")
    
    # Check if the input is a mathutils Matrix
    if not isinstance(matrix, Matrix):
        print("Error: Input is not a mathutils Matrix.")
        return

    # Ensure the matrix is at least 3x3 to avoid indexing errors
    # We explicitly iterate up to 3 for rows and columns (0, 1, 2)
    for i in range(3): # Iterate over rows
        row_values = []
        for j in range(3): # Iterate over columns
            # Access the element at matrix[row_index][col_index]
            # Use f-string formatting to control decimal precision
            row_values.append(f"{matrix[i][j]:.4f}")
        
        # Print the formatted row
        print(f"| {' '.join(row_values)} |")

    print("-------------------------")

def copy_and_negate_rotation():
    try:
        col_left = bpy.data.collections["left"]
        col_right = bpy.data.collections["right"]
    except KeyError as e:
        print(f"Error: Collection '{e.args[0]}' not found. Script aborted.")
        return

    print("\n--- Starting Rotation Copy and Negation ---")
    
    for obj_left in col_left.objects:
        
        # Only process mesh objects
        if obj_left.type != 'MESH':
            continue

        rname = "r" + obj_left.name
        
        # 3. Try to find the corresponding object in the "right" collection
        obj_right = col_right.objects.get(rname)
        print(rname)
        if obj_right and obj_right.type == 'MESH':
            
            loc = obj_left.location.copy()
            
            M_x = Matrix([
                ( -1.0,  0.0,  0.0),
                ( 0.0, 1.0,  0.0),
                ( 0.0,  0.0, 1.0)
            ])
            
            M_z = Matrix([
                (1.0,  0.0,  0.0),
                ( 0.0, 1.0,  0.0),
                ( 0.0,  0.0, -1.0)
            ])
            
            R_A = obj_left.matrix_world.to_3x3()
            R_B = M_z @ R_A @ M_x
            print_matrix_3x3(R_A)
            print_matrix_3x3(R_B)
            if obj_right.rotation_mode == 'QUATERNION':
                obj_right.rotation_quaternion = R_B.to_quaternion()
                print('quat')
            else: 
                rot_B_euler = R_B.to_euler(obj_left.rotation_euler.order)
                obj_right.rotation_euler = rot_B_euler
                print('euler')
            
            obj_right.location = loc
            obj_right.location.z*=-1
            
        else:
            print(f"Warning: Corresponding mesh object '{rname}' not found in 'right' collection.")

        #break
    print("--- Rotation Update Complete ---\n")

# Execute the function
copy_and_negate_rotation()