# exports each selected object into its own file

import bpy
import os

# export to blend file location
basedir = os.path.dirname(bpy.data.filepath)
ourDir = os.path.join(basedir, "right")
try:
    os.makedirs(ourDir, exist_ok=True)
except OSError as e:
    print(f"Error creating directory {ourDir}: {e}")    
if not basedir:
    raise Exception("Blend file is not saved")

view_layer = bpy.context.view_layer

obj_active = view_layer.objects.active
selection = bpy.context.selected_objects

bpy.ops.object.select_all(action='DESELECT')

print("saving " + str(len(selection)) + " files")
for obj in selection:

    obj.select_set(True)

    # some exporters only use the active object
    view_layer.objects.active = obj

    name = bpy.path.clean_name(obj.name)
    fn = os.path.join(ourDir, name)

    bpy.ops.wm.obj_export(filepath=fn + ".obj",    
        forward_axis='Y',
        up_axis='Z',        
        # EXPORT OPTIONS
        export_normals=False,        
        export_materials=False,
        export_selected_objects=True,
        export_vertex_groups=False,
        export_smooth_groups=False,
        export_pbr_extensions=False)

    obj.select_set(False)

    print("written:", fn)

print("done")
view_layer.objects.active = obj_active

for obj in selection:
    obj.select_set(True)
