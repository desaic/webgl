"""Extract an x-slice of saved packs and audit medium targets near a point.

Run with plain Python; no Blender dependency. Paths are intentionally supplied on
the command line so the same audit can be reused for another opening.
"""
import argparse
import csv
import json
import math
from pathlib import Path


def read_pack(path):
    lines = path.read_text().splitlines()
    fruits = []
    for line in lines[1:]:
        tok = line.split()
        if len(tok) < 6 or tok[1] != "pos":
            continue
        fruits.append((tok[0], tuple(map(float, tok[2:5])), line))
    if int(lines[0]) != len(fruits):
        raise ValueError(f"{path}: header count differs from parsed fruit count")
    return fruits


def read_stats(path):
    result = {}
    for line in path.read_text().splitlines():
        tok = line.split()
        if len(tok) >= 12 and tok[1] == "box":
            size = list(map(float, tok[9:12]))
            result[tok[0]] = math.sqrt(sum(x * x for x in size))
    return result


def distance(a, b):
    return math.dist(a, b)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pack", type=Path, action="append", required=True)
    parser.add_argument("--targets", type=Path)
    parser.add_argument("--stats", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--x-max", type=float, default=-151.0)
    parser.add_argument("--center", type=float, nargs=3,
                        default=(-168.154, -60.4725, -48.5998))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    center = tuple(args.center)
    diagonals = read_stats(args.stats)
    report = {"center": center, "x_max": args.x_max, "packs": []}
    for path in args.pack:
        fruits = read_pack(path)
        selected = [f for f in fruits if f[1][0] < args.x_max]
        outfile = args.output / f"{path.parent.name}_{path.stem}_x_lt_{abs(args.x_max):g}.txt"
        outfile.write_text(str(len(selected)) + "\n" +
                           "\n".join(f[2] for f in selected) + "\n")
        near = sorted(fruits, key=lambda f: distance(f[1], center))[:40]
        report["packs"].append({
            "source": str(path), "debug_pack": str(outfile),
            "total_count": len(fruits), "slice_count": len(selected),
            "within_10_cm": sum(distance(f[1], center) <= 10 for f in fruits),
            "within_20_cm": sum(distance(f[1], center) <= 20 for f in fruits),
            "within_30_cm": sum(distance(f[1], center) <= 30 for f in fruits),
            "nearest": [{"name": f[0], "center": f[1],
                         "distance_cm": round(distance(f[1], center), 3),
                         "diagonal_cm": round(diagonals.get(f[0], -1), 3)}
                        for f in near],
        })
    if args.targets:
        with args.targets.open(newline="") as file:
            targets = list(csv.DictReader(file))
        ranked = sorted(targets, key=lambda t: distance(
            (float(t["x"]), float(t["y"]), float(t["z"])), center))
        local = [t for t in ranked if distance(
            (float(t["x"]), float(t["y"]), float(t["z"])), center) <= 30]
        target_file = args.output / "local_targets_within_30cm.csv"
        if local:
            with target_file.open("w", newline="") as file:
                writer = csv.DictWriter(file, fieldnames=local[0].keys())
                writer.writeheader()
                writer.writerows(local)
        report["targets"] = {
            "source": str(args.targets), "total_count": len(targets),
            "local_file": str(target_file),
            "counts_by_distance_cm": {str(r): sum(distance(
                (float(t["x"]), float(t["y"]), float(t["z"])), center) <= r
                for t in targets) for r in (5, 10, 15, 20, 30)},
            "nearest": ranked[:20],
        }
    (args.output / "audit.json").write_text(json.dumps(report, indent=2) + "\n")
    for p in report["packs"]:
        print(p["source"], "total", p["total_count"], "slice", p["slice_count"],
              "within 10/20/30", p["within_10_cm"], p["within_20_cm"],
              p["within_30_cm"])
        for f in p["nearest"][:10]:
            print(" ", f["name"], f["center"], f["distance_cm"], f["diagonal_cm"])
    if "targets" in report:
        print("targets by radius", report["targets"]["counts_by_distance_cm"])
        for t in report["targets"]["nearest"][:10]:
            print(" ", t)


if __name__ == "__main__":
    main()
