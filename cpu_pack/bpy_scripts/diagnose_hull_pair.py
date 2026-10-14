"""Check whether a local target cluster lies inside a two-fruit convex hull."""
import sys
from pathlib import Path

import numpy as np
from scipy.spatial import ConvexHull


pack, mesh_dir, targets = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
names = sys.argv[4:6]
fruit_points = []
fruit_owner = []
for line in pack.read_text().splitlines()[1:]:
    t = line.split()
    if t[0] not in names:
        continue
    pos = np.asarray(list(map(float, t[t.index("pos") + 1:t.index("pos") + 4])))
    rot = np.asarray(list(map(float, t[t.index("rot") + 1:t.index("rot") + 10]))).reshape(3, 3)
    vertices = np.asarray([[float(v) for v in l.split()[1:4]]
                           for l in (mesh_dir / (t[0] + ".obj")).read_text().splitlines()
                           if l.startswith("v ")])
    stride = max(1, len(vertices) // 300)
    sampled = vertices[::stride] @ rot.T + pos
    fruit_points.extend(sampled)
    fruit_owner.extend([t[0]] * len(sampled))
hull = ConvexHull(np.asarray(fruit_points))
cloud = np.asarray([[float(v) for v in l.split()[1:4]]
                    for l in targets.read_text().splitlines() if l.startswith("v ")])
center = np.asarray((-222.72, -48.77, -52.04))
local = cloud[np.linalg.norm(cloud - center, axis=1) < 10]
signed = (local @ hull.equations[:, :3].T + hull.equations[:, 3]).max(axis=1)
print("fruit", names, "hull facets", len(hull.simplices), "local targets", len(local))
print("inside", sum(signed <= 0), "outside", sum(signed > 0),
      "signed margin quantiles", np.round(np.quantile(signed, [0, .25, .5, .75, 1]), 2))
for p, d in sorted(zip(local, signed), key=lambda x: x[1], reverse=True)[:10]:
    print(np.round(p, 2), round(float(d), 2))

# Check which mixed-owner hull facets are actually over the missed targets.
triangles = np.asarray(fruit_points)[hull.simplices]
mixed = np.asarray([len({fruit_owner[i] for i in tri}) > 1 for tri in hull.simplices])
normals = hull.equations[:, :3]
hits = [[] for _ in local]
for fi, (tri, normal) in enumerate(zip(triangles, normals)):
    if not mixed[fi]:
        continue
    a, b, c = tri
    ab, ac = b - a, c - a
    gram = np.array([[ab @ ab, ab @ ac], [ab @ ac, ac @ ac]])
    if abs(np.linalg.det(gram)) < 1e-8:
        continue
    depth = np.sum((a - local) * normal, axis=1)
    projected = local + depth[:, None] * normal
    uv = np.linalg.solve(gram, np.stack(((projected-a) @ ab, (projected-a) @ ac)))
    mask = (depth > 0) & (depth < 16) & (uv[0] >= -.12) & (uv[1] >= -.12) & (uv[0]+uv[1] <= 1.12)
    for pi in np.flatnonzero(mask):
        hits[pi].append((fi, depth[pi], projected[pi]))
print('targets with mixed facet projection', sum(bool(h) for h in hits), '/', len(local))
from collections import Counter
counts = Counter(fi for h in hits for fi, _, _ in h)
for fi, count in counts.most_common(8):
    points = [point for h in hits for facet, _, point in h if facet == fi]
    depths = [depth for h in hits for facet, depth, _ in h if facet == fi]
    print('facet', fi, 'count', count, 'centroid', np.round(triangles[fi].mean(axis=0), 2),
          'projected', np.round(np.mean(points, axis=0), 2), 'normal', np.round(normals[fi], 2),
          'depth', np.round([min(depths), max(depths)], 2))
