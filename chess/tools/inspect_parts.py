# blender -b ignore/pieces.blend --python tools/inspect_parts.py
# Lists mesh datablocks and the connected components (loose parts) of each mesh with size info.
import bpy
import bmesh

print("=== meshes ===", [(m.name, m.users, len(m.polygons)) for m in bpy.data.meshes])
for o in bpy.data.objects:
    if o.type != "MESH":
        continue
    print("object", o.name, "matrix", [list(r) for r in o.matrix_world], "materials", [s.material.name if s.material else None for s in o.material_slots])
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table()
    print("verts", len(bm.verts), "faces", len(bm.faces))
    seen = bytearray(len(bm.verts))
    parts = []
    for v0 in bm.verts:
        if seen[v0.index]:
            continue
        stack = [v0]
        seen[v0.index] = 1
        n = 0
        lo = [1e30] * 3
        hi = [-1e30] * 3
        while stack:
            v = stack.pop()
            n += 1
            for i in range(3):
                lo[i] = min(lo[i], v.co[i])
                hi[i] = max(hi[i], v.co[i])
            for e in v.link_edges:
                w = e.other_vert(v)
                if not seen[w.index]:
                    seen[w.index] = 1
                    stack.append(w)
        parts.append((n, lo, hi))
    print("loose parts:", len(parts))
    for n, lo, hi in sorted(parts, key=lambda p: p[1][0])[:60]:
        print(f"  verts={n:8} lo={[round(c, 2) for c in lo]} size={[round(h - l, 2) for l, h in zip(lo, hi)]}")
    bm.free()
