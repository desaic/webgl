"""List connected clusters of unclaimed crevice-face target points."""
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree


path = Path(sys.argv[1])
points = np.asarray([[float(v) for v in line.split()[1:4]]
                     for line in path.read_text().splitlines()
                     if line.startswith("v ")])
parent = list(range(len(points)))


def root(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]
        i = parent[i]
    return i


for a, b in cKDTree(points).query_pairs(2.3):
    parent[root(a)] = root(b)
groups = defaultdict(list)
for i in range(len(points)):
    groups[root(i)].append(i)
for ids in sorted(groups.values(), key=len, reverse=True)[:5 if len(sys.argv) > 3 else 20]:
    cluster = points[ids]
    center = cluster.mean(axis=0)
    print(len(ids), "center", np.round(center, 2),
          "bounds", np.round(cluster.min(axis=0), 1),
          np.round(cluster.max(axis=0), 1))
    if len(sys.argv) > 3:
        pack_lines = Path(sys.argv[2]).read_text().splitlines()[1:]
        mesh_dir = Path(sys.argv[3])
        nearest = []
        for line in pack_lines:
            t = line.split()
            name = t[0]
            pos = np.asarray(list(map(float, t[t.index("pos") + 1:t.index("pos") + 4])))
            r = np.asarray(list(map(float, t[t.index("rot") + 1:t.index("rot") + 10]))).reshape(3, 3)
            vertices = np.asarray([[float(v) for v in l.split()[1:4]]
                                   for l in (mesh_dir / (name + ".obj")).read_text().splitlines()
                                   if l.startswith("v ")])
            dist = cKDTree(vertices).query((center - pos) @ r)[0]
            nearest.append((round(float(dist), 2), name))
        print("  nearest fruit vertices", sorted(nearest)[:4])
