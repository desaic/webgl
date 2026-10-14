"""Approximate sphere-to-fruit and sphere-to-sphere gaps from mesh vertices.

python audit_sphere_cover.py PACK MESH_DIR SPHERES
"""
import sys
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

pack, mesh_dir, sphere_file = map(Path, sys.argv[1:4])
spheres = np.loadtxt(sphere_file, skiprows=1)
centers, radii = spheres[:, :3], spheres[:, 3]
vertices = []
for line in pack.read_text().splitlines()[1:]:
    t = line.split()
    name = t[0]
    pos = np.asarray(list(map(float, t[t.index("pos") + 1:t.index("pos") + 4])))
    rot = np.asarray(list(map(float, t[t.index("rot") + 1:t.index("rot") + 10]))).reshape(3, 3)
    scale = float(t[t.index("scale") + 1])
    points = np.asarray([[float(x) for x in l.split()[1:4]] for l in
                         (mesh_dir / (name + ".obj")).read_text().splitlines()
                         if l.startswith("v ")])
    vertices.append(scale * (points @ rot.T) + pos)
tree = cKDTree(np.concatenate(vertices))
fruit_gap = tree.query(centers)[0] - radii
dist = np.linalg.norm(centers[:, None, :] - centers[None, :, :], axis=2)
pair_gap = dist - radii[:, None] - radii[None, :]
np.fill_diagonal(pair_gap, np.inf)
nearest_sphere_gap = pair_gap.min(axis=1)
nearest_any_gap = np.minimum(fruit_gap, nearest_sphere_gap)
for label, values in (("fruit", fruit_gap), ("sphere", nearest_sphere_gap),
                      ("any", nearest_any_gap)):
    print(label, "gap cm quantiles", np.round(np.quantile(values, [0, .1, .25, .5, .75, .9, 1]), 3),
          "count >.2", int(np.sum(values > .2)), "count >.5", int(np.sum(values > .5)))
