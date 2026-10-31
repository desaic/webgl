# blender -b ignore/pieces.blend --python tools/inspect_blend.py
# Lists every object with its type, parent, triangle count, bounding box and modifiers.
import bpy

print("=== objects ===")
for o in bpy.data.objects:
    tris = 0
    if o.type == "MESH":
        o.data.calc_loop_triangles()
        tris = len(o.data.loop_triangles)
    bb = [tuple(round(c, 3) for c in o.matrix_world @ __import__("mathutils").Vector(v)) for v in list(o.bound_box)[::7]] if o.type == "MESH" else ""
    mods = [m.type for m in o.modifiers]
    print(f"{o.name!r:28} {o.type:8} parent={o.parent.name if o.parent else None!s:14} tris={tris:8} "
          f"hide={o.hide_viewport}/{o.hide_render} mods={mods} bbox={bb}")
print("collections:", [c.name for c in bpy.data.collections])
print("scene units:", bpy.context.scene.unit_settings.system, bpy.context.scene.unit_settings.scale_length)
