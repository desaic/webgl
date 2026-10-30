"""Convex decomposition (CoACD command line) of every fruit mesh into a folder.

usage: python -I coacd_batch.py [SRC_DIR] [OUT_DIR] [NAME,NAME,...]

Per mesh, in OUT_DIR (same layout as the vhacd output of cpu_pack --vhacd):
  name.obj        preview, one object (o hull_k) per convex hull
  name.hulls.txt  "hulls N", then per hull "hull V F", V lines "x y z", F lines "a b c"
                  (0-based indices into that hull)
Mirrored meshes (name_m) are skipped like the packer does. The CoACD main.exe is built from
F:/github/coacd.
"""
import os
import subprocess
import sys
import tempfile
import time

EXE = "F:/github/coacd/build/main.exe"
SRC = "F:/meshes/fruit_hand/fruits_1"
OUT = "F:/meshes/fruit_hand/fruit_coacd"
# medium to low fidelity: coarse threshold, few hulls, few vertices per hull.
# the manifold preprocess resolution (-pr) dominates the run time: the remeshed surface at the
# default 50 has about 40k triangles and takes 100 s per fruit, at 12 about 4k and 5 s.
ARGS = ["--threshold", "0.08", "--max-convex-hull", "8", "--decimate", "--max-ch-vertex", "48",
        "--seed", "1", "-pr", "12", "-mi", "20", "-md", "2", "-mn", "10", "-r", "1000"]


def read_hulls(path):
    """hulls of a CoACD obj: vertices are global, faces refer to them, objects split the hulls."""
    verts = []
    hulls = []  # list of list of faces (global indices)
    for line in open(path):
        t = line.split()
        if not t:
            continue
        if t[0] == "v":
            verts.append([float(x) for x in t[1:4]])
        elif t[0] in ("o", "g"):
            hulls.append([])
        elif t[0] == "f":
            if not hulls:
                hulls.append([])
            hulls[-1].append([int(x.split("/")[0]) - 1 for x in t[1:4]])
    out = []
    for faces in hulls:
        if not faces:
            continue
        used = sorted({i for f in faces for i in f})
        remap = {g: k for k, g in enumerate(used)}
        out.append(([verts[g] for g in used], [[remap[i] for i in f] for f in faces]))
    return out


def write_outputs(base, hulls):
    with open(base + ".obj", "w") as obj, open(base + ".hulls.txt", "w") as txt:
        txt.write("hulls %d\n" % len(hulls))
        offset = 1
        for k, (pts, tris) in enumerate(hulls):
            obj.write("o hull_%d\n" % k)
            txt.write("hull %d %d\n" % (len(pts), len(tris)))
            for p in pts:
                obj.write("v %.7g %.7g %.7g\n" % tuple(p))
                txt.write("%.7g %.7g %.7g\n" % tuple(p))
            for t in tris:
                obj.write("f %d %d %d\n" % (t[0] + offset, t[1] + offset, t[2] + offset))
                txt.write("%d %d %d\n" % tuple(t))
            offset += len(pts)


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else SRC
    out_dir = sys.argv[2] if len(sys.argv) > 2 else OUT
    only = set(sys.argv[3].split(",")) if len(sys.argv) > 3 else None
    os.makedirs(out_dir, exist_ok=True)
    names = sorted(n for n in os.listdir(src_dir)
                   if n.lower().endswith(".obj") and not n[:-4].endswith("_m")
                   and (only is None or n[:-4] in only))
    print("coacd on", len(names), "meshes from", src_dir, "into", out_dir, flush=True)
    failed = 0
    start = time.time()
    for n in names:
        t0 = time.time()
        with tempfile.TemporaryDirectory() as tmp:
            res = os.path.join(tmp, "out.obj")
            r = subprocess.run([EXE, "-i", os.path.join(src_dir, n), "-o", res,
                                "-ro", os.path.join(tmp, "remesh.obj")] + ARGS,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=tmp)
            if not os.path.exists(res):
                failed += 1
                print("%-28s FAILED (exit %d)" % (n, r.returncode), flush=True)
                continue
            hulls = read_hulls(res)
        if not hulls:
            failed += 1
            print("%-28s FAILED (no hulls)" % n, flush=True)
            continue
        write_outputs(os.path.join(out_dir, n[:-4]), hulls)
        print("%-28s hulls %2d  verts %4d  %.1f s" %
              (n, len(hulls), sum(len(h[0]) for h in hulls), time.time() - t0), flush=True)
    print("done, %d failed, %.0f s" % (failed, time.time() - start))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
