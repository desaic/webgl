"""Resample globally computed crevice targets, then export one overlap window.

python resample_medium_subgrid.py <medium_patch_targets.csv> <output-folder>

The C++ medium pass computes the targets from the entire fixed-support pack.
This script only changes their sampling and graph; it never clips the
shrinkwrap calculation to a subgrid or edits a pack. The debug window has
size 10 fruit diameters and stride 9 diameters (one-diameter overlap).
"""

import csv
import heapq
import json
import math
import random
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def read_points(path):
    with path.open(newline="") as f:
        return [
            (
                (float(r["x"]), float(r["y"]), float(r["z"])),
                (float(r["nx"]), float(r["ny"]), float(r["nz"])),
                float(r["clearance_cm"]),
                int(float(r["wrap_radius_cm"])),
                bool(int(r["initially_covered"])),
            )
            for r in csv.DictReader(f)
        ]


def dist(a, b):
    return math.dist(a, b)


def medium_size_from_stats(path):
    sizes = []
    if path.exists():
        for line in path.read_text().splitlines():
            tokens = line.split()
            if "box" not in tokens:
                continue
            i = tokens.index("box")
            extent = list(map(float, tokens[i + 1:i + 7]))
            if len(extent) != 6:
                continue
            diagonal = math.sqrt(sum((extent[a + 3] - extent[a]) ** 2
                                     for a in range(3)))
            if 8.0 < diagonal <= 14.0:
                sizes.append(diagonal)
    return statistics.median(sizes) if sizes else 10.0


def build_graph(points, cell_size=3.0, max_edge=5.5):
    low = tuple(min(p[0][axis] for p in points) for axis in range(3))
    def cell(i):
        p = points[i][0]
        return (points[i][3],) + tuple(math.floor((p[a] - low[a]) / cell_size) for a in range(3))
    cells = defaultdict(list)
    for i in range(len(points)):
        cells[cell(i)].append(i)
    links = [[] for _ in points]
    for i, point in enumerate(points):
        scale, x, y, z = cell(i)
        for dz in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    for j in cells.get((scale, x + dx, y + dy, z + dz), ()):
                        if j <= i:
                            continue
                        other = points[j]
                        length = dist(point[0], other[0])
                        normal_dot = sum(a * b for a, b in zip(point[1], other[1]))
                        displacement = tuple(other[0][a] - point[0][a] for a in range(3))
                        tangent_a = abs(sum(point[1][a] * displacement[a] for a in range(3)))
                        tangent_b = abs(sum(other[1][a] * displacement[a] for a in range(3)))
                        if (0 < length <= max_edge and normal_dot >= 0.25 and
                                tangent_a <= 0.6 * length and tangent_b <= 0.6 * length):
                            links[i].append((j, length))
                            links[j].append((i, length))
    return links


def blue_noise_on_graph(points, links, spacing):
    """Maximal geodesic Poisson set; each raw node lies near a selected node."""
    rng = random.Random(20261007)
    priority = []
    for i, p in enumerate(points):
        endpoint = len(links[i]) <= 2
        priority.append((-(2.0 if endpoint else 0.0) -
                         0.2 * min(p[2] / spacing, 1.0) - rng.random(), i))
    priority.sort()
    suppressed = bytearray(len(points))
    seeds = []
    for _, i in priority:
        if suppressed[i]:
            continue
        seeds.append(i)
        heap = [(0.0, i)]
        local = {i: 0.0}
        while heap:
            d, u = heapq.heappop(heap)
            if d > local[u] or d >= spacing:
                continue
            suppressed[u] = 1
            for v, length in links[u]:
                nd = d + length
                if nd < spacing and nd < local.get(v, math.inf):
                    local[v] = nd
                    heapq.heappush(heap, (nd, v))
    return seeds


def graph_voronoi(links, seeds):
    owner = [-1] * len(links)
    parent = [-1] * len(links)
    depth = [math.inf] * len(links)
    heap = []
    for node_id, raw_id in enumerate(seeds):
        owner[raw_id] = node_id
        depth[raw_id] = 0.0
        heapq.heappush(heap, (0.0, raw_id))
    while heap:
        d, u = heapq.heappop(heap)
        if d != depth[u]:
            continue
        for v, length in links[u]:
            nd = d + length
            if nd < depth[v]:
                depth[v] = nd
                owner[v] = owner[u]
                parent[v] = u
                heapq.heappush(heap, (nd, v))
    crossings = {}
    for u, neighbors in enumerate(links):
        for v, length in neighbors:
            if v <= u or owner[u] == owner[v]:
                continue
            a, b = sorted((owner[u], owner[v]))
            candidate = (depth[u] + length + depth[v], u, v)
            if candidate < crossings.get((a, b), (math.inf, -1, -1)):
                crossings[(a, b)] = candidate
    return owner, parent, depth, crossings


def path_to_seed(raw_id, parent):
    path = [raw_id]
    while parent[path[-1]] >= 0:
        path.append(parent[path[-1]])
    return path


def write_points_obj(path, locations):
    with path.open("w") as f:
        for x, y, z in locations:
            f.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")


def write_box_obj(path, low, high):
    corners = [(x, y, z) for z in (low[2], high[2])
               for y in (low[1], high[1]) for x in (low[0], high[0])]
    edges = [(1, 2), (1, 3), (2, 4), (3, 4), (5, 6), (5, 7),
             (6, 8), (7, 8), (1, 5), (2, 6), (3, 7), (4, 8)]
    with path.open("w") as f:
        for x, y, z in corners:
            f.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")
        for a, b in edges:
            f.write(f"l {a} {b}\n")


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    fruit_size = (float(sys.argv[3]) if len(sys.argv) > 3 else
                  medium_size_from_stats(source.parent.parent / "fruits_1" / "stats.txt"))
    spacing = 0.7 * fruit_size
    window_size = 10.0 * fruit_size
    overlap = fruit_size
    stride = window_size - overlap
    output.mkdir(parents=True, exist_ok=True)
    points = read_points(source)
    if not points:
        raise RuntimeError("No full-pack crevice samples in CSV")
    links = build_graph(points)
    seeds = blue_noise_on_graph(points, links, spacing)
    owner, parent, depth, crossings = graph_voronoi(links, seeds)
    low = tuple(min(p[0][axis] for p in points) for axis in range(3))
    core_counts = defaultdict(int)
    for p in points:
        if not p[4]:
            core_counts[tuple(math.floor((p[0][a] - low[a]) / stride)
                              for a in range(3))] += 1
    cell = max(core_counts, key=core_counts.get)
    core_low = tuple(low[a] + cell[a] * stride for a in range(3))
    core_high = tuple(v + stride for v in core_low)
    window_low = tuple(v - overlap / 2 for v in core_low)
    window_high = tuple(v + overlap / 2 for v in core_high)
    def inside(p, a, b):
        return all(a[i] <= p[i] < b[i] for i in range(3))
    raw_in_window = [i for i, p in enumerate(points)
                     if inside(p[0], window_low, window_high)]
    selected = [sid for sid, rid in enumerate(seeds)
                if inside(points[rid][0], window_low, window_high)]
    selected_set = set(selected)
    write_points_obj(output / "raw_points.obj", (points[i][0] for i in raw_in_window))
    write_points_obj(output / "resampled_points.obj",
                     (points[seeds[sid]][0] for sid in selected))
    write_box_obj(output / "core_box.obj", core_low, core_high)
    write_box_obj(output / "overlap_window.obj", window_low, window_high)
    owned_count = defaultdict(int)
    for rid in raw_in_window:
        owned_count[owner[rid]] += 1
    degree = defaultdict(int)
    edge_rows = []
    boundary_edges = 0
    with (output / "graph_paths.obj").open("w") as local_obj, \
         (output / "graph_boundary_paths.obj").open("w") as boundary_obj:
        vertices = {False: 0, True: 0}
        for (a, b), (length, u, v) in sorted(crossings.items()):
            if a not in selected_set or b not in selected_set:
                continue
            if owner[u] != a:
                u, v = v, u
            path = list(reversed(path_to_seed(u, parent))) + path_to_seed(v, parent)
            crosses_window = any(not inside(points[raw_id][0], window_low, window_high)
                                 for raw_id in path)
            boundary_edges += crosses_window
            obj = boundary_obj if crosses_window else local_obj
            indices = []
            for raw_id in path:
                x, y, z = points[raw_id][0]
                obj.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")
                vertices[crosses_window] += 1
                indices.append(str(vertices[crosses_window]))
            obj.write("l " + " ".join(indices) + "\n")
            degree[a] += 1
            degree[b] += 1
            edge_rows.append((a, b, length,
                              dist(points[seeds[a]][0], points[seeds[b]][0]),
                              len(path), int(crosses_window)))
    with (output / "nodes.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(("node_id", "x", "y", "z", "wrap_radius_cm", "clearance_cm",
                    "initially_covered", "raw_samples_owned", "degree", "in_core"))
        for sid in selected:
            p = points[seeds[sid]]
            w.writerow((sid, *p[0], p[3], p[2], int(p[4]), owned_count[sid],
                        degree[sid], int(inside(p[0], core_low, core_high))))
    with (output / "edges.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(("node_a", "node_b", "geodesic_cm", "straight_cm",
                    "path_samples", "crosses_window"))
        w.writerows(edge_rows)
    manifest = {
        "source": str(source), "fruit_size_cm": fruit_size,
        "resample_spacing_cm": spacing, "window_size_cm": window_size,
        "overlap_cm": overlap, "grid_stride_cm": stride, "grid_cell": cell,
        "core_low": core_low, "core_high": core_high,
        "window_low": window_low, "window_high": window_high,
        "raw_points_global": len(points), "resampled_points_global": len(seeds),
        "raw_points_window": len(raw_in_window),
        "resampled_points_window": len(selected),
        "resampled_points_core": sum(inside(points[seeds[sid]][0], core_low, core_high)
                                     for sid in selected),
        "initially_covered_nodes_window": sum(points[seeds[sid]][4] for sid in selected),
        "graph_edges_window": len(edge_rows),
        "graph_edges_crossing_window": boundary_edges,
        "median_graph_edge_cm": (statistics.median(row[2] for row in edge_rows)
                                  if edge_rows else 0.0),
        "max_graph_edge_cm": (max(row[2] for row in edge_rows)
                               if edge_rows else 0.0),
        "uncovered_raw_points_core": core_counts[cell],
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2))
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
