"""Measure distances from local target points to placed fruit surfaces."""
import argparse
import csv
import math
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

from audit_medium_hole import read_pack, read_stats


def mesh_tree(path):
    vertices = []
    with path.open() as file:
        for line in file:
            if line.startswith("v "):
                vertices.append(tuple(map(float, line.split()[1:4])))
    return cKDTree(np.asarray(vertices, dtype=float))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--targets", type=Path, required=True)
    parser.add_argument("--stats", type=Path, required=True)
    parser.add_argument("--mesh-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--center", type=float, nargs=3,
                        default=(-168.154, -60.4725, -48.5998))
    args = parser.parse_args()
    stats = read_stats(args.stats)
    center = np.asarray(args.center)
    with args.targets.open(newline="") as file:
        targets = [row for row in csv.DictReader(file)
                   if math.dist(center, [float(row[k]) for k in "xyz"]) <= 10
                   and row["initially_covered"] == "0"]
    if not targets:
        raise ValueError("No local uncovered targets")
    xyz = np.asarray([[float(row[k]) for k in "xyz"] for row in targets])
    instances = []
    for name, pos, line in read_pack(args.pack):
        if math.dist(pos, center) > 38:
            continue
        token = line.split()
        ri = token.index("rot") + 1
        rotation = np.asarray(list(map(float, token[ri:ri + 9]))).reshape((3, 3))
        instances.append((name, np.asarray(pos), rotation, stats.get(name, 0)))
    trees = {}
    nearest_large = np.full(len(targets), np.inf)
    nearest_any = np.full(len(targets), np.inf)
    large_name = [""] * len(targets)
    any_name = [""] * len(targets)
    for name, pos, rotation, diagonal in instances:
        if name not in trees:
            trees[name] = mesh_tree(args.mesh_dir / (name + ".obj"))
        local = (xyz - pos) @ rotation
        distances, _ = trees[name].query(local)
        for i, d in enumerate(distances):
            if d < nearest_any[i]:
                nearest_any[i], any_name[i] = d, name
            if diagonal > 14 and d < nearest_large[i]:
                nearest_large[i], large_name[i] = d, name
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as file:
        fields = list(targets[0].keys()) + ["nearest_any_cm", "nearest_any_name",
                                            "nearest_large_cm", "nearest_large_name"]
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        for i, row in enumerate(targets):
            row.update(nearest_any_cm=round(float(nearest_any[i]), 3),
                       nearest_any_name=any_name[i],
                       nearest_large_cm=round(float(nearest_large[i]), 3),
                       nearest_large_name=large_name[i])
            writer.writerow(row)
    print("local initially open targets", len(targets))
    for cutoff in (1, 2, 3, 4, 5, 6):
        print("farther than", cutoff, "cm from large fruit:",
              int(np.count_nonzero(nearest_large > cutoff)))
    for i in np.argsort(nearest_large)[-12:]:
        print("target", np.round(xyz[i], 2), "any", any_name[i],
              round(float(nearest_any[i]), 2), "large", large_name[i],
              round(float(nearest_large[i]), 2))


if __name__ == "__main__":
    main()
