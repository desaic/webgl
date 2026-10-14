"""Audit marginal volume and crevice coverage of every saved sphere."""
import sys
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

folder = Path(sys.argv[1])
spheres = np.loadtxt(folder / "sphere_cover.txt", skiprows=1)
centers, radii = spheres[:, :3], spheres[:, 3]


def obj_points(path):
    return np.array([[float(v) for v in line.split()[1:4]]
                     for line in path.read_text().splitlines() if line.startswith("v ")])


targets = obj_points(folder / "sphere_cover_targets.obj")
mouth_rows = np.loadtxt(folder / "crevice_mouth_points.txt", skiprows=1)
medium = mouth_rows[mouth_rows[:, 10] > 0.5, :3]
all_mouths = mouth_rows[:, :3]
all_tree = cKDTree(all_mouths)
medium_tree = cKDTree(medium)
target_to_mouth = all_tree.query(targets)[0]
target_to_medium = medium_tree.query(targets)[0]

assigned_targets = np.full(len(targets), -1, dtype=int)
assigned_mouths = np.full(len(medium), -1, dtype=int)
rows = []
for i, (c, r) in enumerate(zip(centers, radii)):
    hit_t = np.flatnonzero(np.linalg.norm(targets - c, axis=1) <= r + 0.05)
    hit_m = np.flatnonzero(np.linalg.norm(medium - c, axis=1) <= r + 0.05)
    gained_t = hit_t[assigned_targets[hit_t] < 0]
    gained_m = hit_m[assigned_mouths[hit_m] < 0]
    assigned_targets[gained_t] = i
    assigned_mouths[gained_m] = i
    prev_gap = np.min(np.linalg.norm(centers[:i] - c, axis=1) - radii[:i] - r) if i else np.inf
    rows.append((i, r, len(gained_t), len(gained_m),
                 float(np.min(target_to_mouth[gained_t])) if len(gained_t) else np.inf,
                 float(np.min(target_to_medium[gained_t])) if len(gained_t) else np.inf,
                 float(all_tree.query(c)[0]), float(medium_tree.query(c)[0]),
                 float(prev_gap)))

arr = np.asarray(rows)
print("spheres", len(rows), "targets", len(targets), "medium mouths", len(medium))
print("total assigned targets", np.sum(assigned_targets >= 0),
      "medium mouths", np.sum(assigned_mouths >= 0))
for label, sel in (("all", arr[:, 0] >= 0),):
    x = arr[sel]
    print(label, "spheres", len(x), "no new medium mouths", int(np.sum(x[:, 3] == 0)),
          "no mouth and <=2 targets", int(np.sum((x[:, 3] == 0) & (x[:, 2] <= 2))),
          "new targets", int(np.sum(x[:, 2])), "new mouths", int(np.sum(x[:, 3])))
    print("  no-mouth spheres with center >2.5cm from all mouths",
          int(np.sum((x[:, 3] == 0) & (x[:, 6] > 2.5))))
    print("  no-mouth sphere target gain", int(np.sum(x[x[:, 3] == 0, 2])))
print("targets >2cm from all detected mouth centers", int(np.sum(target_to_mouth > 2)),
      "assigned", int(np.sum((target_to_mouth > 2) & (assigned_targets >= 0))))
print("targets >4cm from all detected mouth centers", int(np.sum(target_to_mouth > 4)),
      "assigned", int(np.sum((target_to_mouth > 4) & (assigned_targets >= 0))))
print("thin no-mouth spheres:")
for row in rows:
    i, r, nt, nm, dta, dtm, dca, dcm, gap = row
    if nm == 0 and nt <= 2:
        print(i, "center", np.round(centers[i], 3), "r", round(r, 3),
              "new targets", nt, "nearest mouth", round(dca, 2),
              "nearest medium mouth", round(dcm, 2), "previous sphere gap", round(gap, 3))
