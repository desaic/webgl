"""Explain one sphere in a saved cover using its sequential coverage and contacts.

python diagnose_sphere_cover.py OUTPUT_DIR X Y Z
"""
import sys
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree


out_dir = Path(sys.argv[1])
query = np.array(list(map(float, sys.argv[2:5])))
spheres = np.loadtxt(out_dir / "sphere_cover.txt", skiprows=1)
centers, radii = spheres[:, :3], spheres[:, 3]
idx = int(np.argmin(np.linalg.norm(centers - query, axis=1)))
print("sphere index", idx, "center", centers[idx], "radius", radii[idx])


def obj_points(path):
    return np.array([[float(v) for v in line.split()[1:4]]
                     for line in path.read_text().splitlines() if line.startswith("v ")])


targets = obj_points(out_dir / "sphere_cover_targets.obj")
mouth_rows = np.loadtxt(out_dir / "crevice_mouth_points.txt", skiprows=1)
mouths = mouth_rows[mouth_rows[:, 10] > 0.5, :3]
for label, points in (("target", targets), ("medium mouth", mouths)):
    d = np.linalg.norm(points - centers[idx], axis=1)
    inside = np.flatnonzero(d <= radii[idx] + 0.05)
    earlier = np.zeros(len(points), dtype=bool)
    for c, r in zip(centers[:idx], radii[:idx]):
        earlier |= np.linalg.norm(points - c, axis=1) <= r + 0.05
    gained = inside[~earlier[inside]]
    print(label, "inside", len(inside), "new at placement", len(gained))
    for j in gained[:20]:
        print(" ", points[j], "distance", round(float(d[j]), 3))

sd = np.linalg.norm(centers - centers[idx], axis=1) - radii - radii[idx]
sd[idx] = np.inf
for j in np.argsort(sd)[:5]:
    print("neighbor", j, centers[j], "radius", radii[j], "gap", round(float(sd[j]), 3),
          "placed earlier", j < idx)

fruit_pack = out_dir / "sphere_cover_fruit.txt"
mesh_dir = Path("F:/meshes/fruit_hand/fruits_1")
best = (np.inf, None, None)
for fid, line in enumerate(fruit_pack.read_text().splitlines()[1:]):
    t = line.split()
    name = t[0]
    pos = np.array(list(map(float, t[t.index("pos") + 1:t.index("pos") + 4])))
    rot = np.array(list(map(float, t[t.index("rot") + 1:t.index("rot") + 10]))).reshape(3, 3)
    scale = float(t[t.index("scale") + 1])
    vertices = obj_points(mesh_dir / (name + ".obj"))
    world = scale * (vertices @ rot.T) + pos
    dist, nearest = cKDTree(world).query(centers[idx])
    if dist < best[0]:
        best = (dist, (fid, name, pos), world[nearest])
print("nearest fruit vertex gap", round(float(best[0] - radii[idx]), 3),
      "fruit", best[1], "vertex", best[2])
