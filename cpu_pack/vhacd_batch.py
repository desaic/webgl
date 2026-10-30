"""Convex decomposition (V-HACD 2.2) of every fruit mesh into a folder of obj files.

usage: python -I vhacd_batch.py [SRC_DIR] [OUT_DIR]

Each SRC_DIR/name.obj becomes OUT_DIR/name.obj with one object (o hull_k) per convex hull, in
the same coordinate frame as the source. Uses the prebuilt testVHACD.exe, which writes VRML,
and converts it to obj. Mirrored meshes (name_m) are skipped like the packer does.
"""
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

EXE = "F:/github/v-hacd/bin/win64/testVHACD.exe"
SRC = "F:/meshes/fruit_hand/fruits_1"
OUT = "F:/meshes/fruit_hand/fruit_vhacd"
# medium to low fidelity: coarse concavity, few vertices per hull.
ARGS = ["--resolution", "100000", "--depth", "12", "--concavity", "0.01",
        "--maxNumVerticesPerCH", "32", "--oclAcceleration", "0"]


def read_obj_bbox(path):
    lo = [1e30] * 3
    hi = [-1e30] * 3
    with open(path) as f:
        for line in f:
            if line.startswith("v "):
                p = [float(v) for v in line.split()[1:4]]
                for k in range(3):
                    lo[k] = min(lo[k], p[k])
                    hi[k] = max(hi[k], p[k])
    return lo, hi


def parse_wrl(path):
    """returns a list of (points, triangles) per convex hull."""
    text = open(path).read()
    hulls = []
    for shape in text.split("Shape {")[1:]:
        m = re.search(r"point \[(.*?)\]", shape, re.S)
        c = re.search(r"coordIndex \[(.*?)\]", shape, re.S)
        if not m or not c:
            continue
        pts = [[float(v) for v in row.split()] for row in m.group(1).split(",") if row.strip()]
        idx = [int(v) for v in c.group(1).replace(",", " ").split()]
        tris = []
        cur = []
        for i in idx:
            if i < 0:
                if len(cur) >= 3:
                    for k in range(1, len(cur) - 1):
                        tris.append((cur[0], cur[k], cur[k + 1]))
                cur = []
            else:
                cur.append(i)
        hulls.append((pts, tris))
    return hulls


def write_obj(path, hulls):
    base = 1
    with open(path, "w") as f:
        for k, (pts, tris) in enumerate(hulls):
            f.write("o hull_%d\n" % k)
            for p in pts:
                f.write("v %.6f %.6f %.6f\n" % (p[0], p[1], p[2]))
            for t in tris:
                f.write("f %d %d %d\n" % (t[0] + base, t[1] + base, t[2] + base))
            base += len(pts)


def run_one(name, src_dir, out_dir):
    src = os.path.join(src_dir, name)
    out = os.path.join(out_dir, name)
    with tempfile.TemporaryDirectory() as tmp:
        wrl = os.path.join(tmp, "out.wrl")
        log = os.path.join(tmp, "vh.log")
        r = subprocess.run([EXE, "--input", src, "--output", wrl, "--log", log] + ARGS,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if not os.path.exists(wrl):
            return name, None, "no output (exit %d)" % r.returncode
        hulls = parse_wrl(wrl)
    if not hulls:
        return name, None, "no hulls"
    write_obj(out, hulls)
    lo0, hi0 = read_obj_bbox(src)
    lo1, hi1 = read_obj_bbox(out)
    dev = max(max(abs(lo0[k] - lo1[k]), abs(hi0[k] - hi1[k])) for k in range(3))
    ext = max(hi0[k] - lo0[k] for k in range(3))
    return name, (len(hulls), sum(len(h[0]) for h in hulls), dev, ext), ""


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else SRC
    out_dir = sys.argv[2] if len(sys.argv) > 2 else OUT
    os.makedirs(out_dir, exist_ok=True)
    names = sorted(n for n in os.listdir(src_dir)
                   if n.lower().endswith(".obj") and not n[:-4].endswith("_m"))
    print("decomposing", len(names), "meshes from", src_dir, "into", out_dir)
    failed = 0
    with ThreadPoolExecutor(max_workers=8) as ex:
        for name, res, err in ex.map(lambda n: run_one(n, src_dir, out_dir), names):
            if res is None:
                failed += 1
                print("%-28s FAILED %s" % (name, err))
            else:
                print("%-28s hulls %2d  verts %4d  bbox dev %.3f cm of %.1f" % ((name,) + res))
    print("done,", failed, "failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
