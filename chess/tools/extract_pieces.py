# Extracts one mesh per piece type from ignore/pieces.blend as plain OBJ (v + f lines only: no normals,
# UVs, colors or materials).
#
#   blender -b ignore/pieces.blend --python tools/extract_pieces.py
#
# The blend has a single object whose mesh is made of 16 loose parts (one set of pieces: 8 pawns,
# 2 each of rook/knight/bishop, queen, king). Parts with the same vertex count are the same piece;
# the six distinct shapes, ordered by height, are pawn < rook < knight < bishop < queen < king.
#
# Output (tools/raw/<name>.obj) is in a three.js friendly frame: Y up, base at y = 0, centred on
# x/z, knight facing +x, units are the blend's own (mm-ish). Scaling to board squares is done by the
# page, simplification by tools/simplify.mjs.
import math
import os

import bmesh
import bpy

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "raw")
NAMES = ["pawn", "rook", "knight", "bishop", "queen", "king"]  # ascending height
# these are modelled upside down in the blend (base at the top)
FLIP = {"pawn", "knight", "king"}
os.makedirs(OUT, exist_ok=True)

obj = next(o for o in bpy.data.objects if o.type == "MESH")
bm = bmesh.new()
bm.from_mesh(obj.data)
bm.verts.ensure_lookup_table()

# connected components by vertex adjacency
comp = [-1] * len(bm.verts)
parts = []
for v0 in bm.verts:
    if comp[v0.index] >= 0:
        continue
    cid = len(parts)
    members = []
    stack = [v0]
    comp[v0.index] = cid
    while stack:
        v = stack.pop()
        members.append(v)
        for e in v.link_edges:
            w = e.other_vert(v)
            if comp[w.index] < 0:
                comp[w.index] = cid
                stack.append(w)
    parts.append(members)

# group identical shapes by vertex count, keep the first of each group
groups = {}
for cid, members in enumerate(parts):
    groups.setdefault(len(members), cid)
assert len(groups) == 6, "expected 6 distinct piece shapes, found %d" % len(groups)


def height(cid):
    zs = [v.co.z for v in parts[cid]]
    return max(zs) - min(zs)


order = sorted(groups.values(), key=height)

for name, cid in zip(NAMES, order):
    verts = parts[cid]
    xs = [v.co.x for v in verts]
    ys = [v.co.y for v in verts]
    zs = [v.co.z for v in verts]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    z0 = min(zs)
    h = max(zs) - z0
    pts = {v.index: [v.co.x - cx, v.co.y - cy, v.co.z - z0] for v in verts}

    if name in FLIP:
        # rotate 180 degrees about the x axis: z -> h - z, y -> -y (keeps the winding)
        for p in pts.values():
            p[1] = -p[1]
            p[2] = h - p[2]

    if name == "knight":
        # the head sticks out toward the front: rotate about the vertical axis so it points to +x
        up = [p for p in pts.values() if p[2] > 0.6 * h]
        mx = sum(p[0] for p in up) / len(up)
        my = sum(p[1] for p in up) / len(up)
        a = -math.atan2(my, mx)
        ca, sa = math.cos(a), math.sin(a)
        for p in pts.values():
            p[0], p[1] = p[0] * ca - p[1] * sa, p[0] * sa + p[1] * ca

    # blend z-up -> y-up: (x, y, z) -> (x, z, -y), a pure rotation so winding is kept
    index = {}
    out_v = []
    for vi, p in pts.items():
        index[vi] = len(out_v) + 1
        out_v.append((p[0], p[2], -p[1]))
    tris = []
    for f in bm.faces:
        if comp[f.verts[0].index] != cid:
            continue
        ids = [index[v.index] for v in f.verts]
        for k in range(1, len(ids) - 1):
            tris.append((ids[0], ids[k], ids[k + 1]))

    # make sure faces point outward (positive signed volume)
    vol = 0.0
    for a_, b_, c_ in tris:
        A, B, C = out_v[a_ - 1], out_v[b_ - 1], out_v[c_ - 1]
        vol += (A[0] * (B[1] * C[2] - B[2] * C[1]) - A[1] * (B[0] * C[2] - B[2] * C[0]) + A[2] * (B[0] * C[1] - B[1] * C[0])) / 6.0
    if vol < 0:
        tris = [(a_, c_, b_) for a_, b_, c_ in tris]

    path = os.path.join(OUT, name + ".obj")
    with open(path, "w") as fh:
        fh.write("# %s: %d vertices, %d triangles, height %.2f\n" % (name, len(out_v), len(tris), h))
        for x, y, z in out_v:
            fh.write("v %.4f %.4f %.4f\n" % (x, y, z))
        for a_, b_, c_ in tris:
            fh.write("f %d %d %d\n" % (a_, b_, c_))
    print("%-7s verts=%7d tris=%7d height=%7.2f width=%.2f volume=%.0f" % (name, len(out_v), len(tris), h, max(xs) - min(xs), abs(vol)))

bm.free()
