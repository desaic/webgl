"""Nudge one fruit of a pack file to sit snugly against the container and its neighbors.
usage: python snug_one.py pack.txt index out.txt [--fruits DIR] [--container STL]
Free space is a voxel flood fill of the crop around the fruit, so no ray casting is needed."""
import sys, argparse
import numpy as np, trimesh
from scipy import ndimage as ndi
from scipy.spatial.transform import Rotation as R

ap = argparse.ArgumentParser()
ap.add_argument("pack"); ap.add_argument("idx", type=int); ap.add_argument("out")
ap.add_argument("--fruits", default="F:/meshes/fruit_hand/fruits_1")
ap.add_argument("--container", default="F:/meshes/fruit_hand/hands/hand4.8m.stl")
ap.add_argument("--dx", type=float, default=0.25)
ap.add_argument("--crop", type=float, default=14.0)
ap.add_argument("--iters", type=int, default=2000)
ap.add_argument("--restarts", type=int, default=8)
a = ap.parse_args()

lines = [l.split() for l in open(a.pack).read().splitlines()[1:] if l.strip()]
def parse(t):
    p = np.array(t[t.index("pos")+1:t.index("pos")+4], float)
    r = np.array(t[t.index("rot")+1:t.index("rot")+10], float).reshape(3, 3)
    s = float(t[t.index("scale")+1]) if "scale" in t else 1.0
    return p, r, s
meshes = {}
def mesh(name):
    if name not in meshes:
        meshes[name] = trimesh.load(f"{a.fruits}/{name}.obj", force="mesh", process=False)
    return meshes[name]
def world(name, p, r, s):
    m = mesh(name)
    return m.vertices * s @ r.T + p, m.faces

k = a.idx if a.idx >= 0 else len(lines) + a.idx
name = lines[k][0]
p0, r0, s0 = parse(lines[k])
kv, kf = world(name, p0, r0, s0)
lo = kv.min(0) - a.crop; hi = kv.max(0) + a.crop
n = np.ceil((hi - lo) / a.dx).astype(int)
surf = np.zeros(n, bool)

def stamp(v, f):
    pts = trimesh.sample.sample_surface(trimesh.Trimesh(v, f, process=False), int(len(f) * 1) + 200000)[0] if False else None
    tri = v[f]
    area = np.linalg.norm(np.cross(tri[:, 1]-tri[:, 0], tri[:, 2]-tri[:, 0]), axis=1) / 2
    cnt = np.maximum(1, np.ceil(area / (a.dx * a.dx * 0.15)).astype(int))
    out = []
    for c in np.unique(cnt):
        sel = np.where(cnt == c)[0]
        u = np.random.rand(c, 2); fl = u.sum(1) > 1; u[fl] = 1 - u[fl]
        w = np.stack([1-u.sum(1), u[:, 0], u[:, 1]], 1)
        out.append(np.einsum("cj,tjk->tck", w, tri[sel]).reshape(-1, 3))
    pts = np.concatenate(out)
    ok = np.all((pts > lo) & (pts < hi - 1e-6), axis=1)
    ij = ((pts[ok] - lo) / a.dx).astype(int)
    surf[ij[:, 0], ij[:, 1], ij[:, 2]] = True

c = trimesh.load(a.container, force="mesh", process=False)
cv = c.vertices; cf = c.faces
m = np.any((cv[cf] > lo) & (cv[cf] < hi), axis=2).all(1) | np.any(np.all((cv[cf] > lo) & (cv[cf] < hi), axis=2), axis=1)
inbox = np.all(cv[cf].max(1) > lo, axis=1) & np.all(cv[cf].min(1) < hi, axis=1)
stamp(cv, cf[inbox])
for i, t in enumerate(lines):
    if i == k: continue
    v, f = world(t[0], *parse(t))
    if np.all(v.max(0) > lo) and np.all(v.min(0) < hi): stamp(v, f)
surf = ndi.binary_dilation(surf)
lab, nl = ndi.label(~surf)
cidx = ((p0 + (kv.mean(0) - p0) - lo) / a.dx).astype(int)
cl = lab[tuple(cidx)]
print("kiwi centre label", cl, "of", nl, "surf voxels", surf.sum())
free = lab == cl
print("free voxels vol", free.sum()*a.dx**3, "kiwi vol", mesh(name).volume*s0**3)
d_out = ndi.distance_transform_edt(free) * a.dx          # distance to obstacle inside free
d_in = ndi.distance_transform_edt(~free) * a.dx
sd = d_out - d_in                                         # >0 free, <0 obstacle (wall or other fruit)

# kiwi surface samples in local frame
km = mesh(name)
ks = trimesh.sample.sample_surface(km, 4000, seed=1)[0] * s0
def sdf_at(pts):
    return ndi.map_coordinates(sd, ((pts - lo) / a.dx - 0.5).T, order=1, mode="nearest")
def cost(x):
    rot = R.from_rotvec(x[3:]) * R.from_matrix(r0)
    pts = ks @ rot.as_matrix().T + x[:3]
    s = sdf_at(pts)
    pen = np.clip(0.1 - s, 0, None)
    gap = np.clip(s, 0, 2.0)
    return 50 * (pen**2).sum() + (gap).sum() * 0.05, (s < -0.05).sum(), s.min(), gap.mean()
def run(seed, x):
    best = cost(x)
    rng = np.random.default_rng(seed); sp, sr = 1.0, 0.3
    for it in range(a.iters):
        y = x + np.concatenate([rng.normal(0, sp, 3), rng.normal(0, sr, 3)])
        cy = cost(y)
        if cy[0] < best[0]: x, best = y, cy
        if it % 500 == 499: sp *= 0.6; sr *= 0.6
    return x, best
print("start", cost(np.concatenate([p0, np.zeros(3)])))
res = []
for seed in range(a.restarts):
    x0 = np.concatenate([p0, np.zeros(3) if seed == 0 else np.random.default_rng(seed).normal(0, 1.0, 3)])
    x, b = run(seed, x0); res.append((b[0], seed, x, b)); print(seed, b)
_, _, x, best = min(res, key=lambda t: t[0])
print("end", best)
rot = (R.from_rotvec(x[3:]) * R.from_matrix(r0)).as_matrix()
t = lines[k]
tok = [name, "pos", *[f"{v:.6f}" for v in x[:3]], "rot", *[f"{v:.6f}" for v in rot.reshape(-1)], "scale", f"{s0:g}"]
L = open(a.pack).read().splitlines()
L[1 + k] = " ".join(tok)
open(a.out, "w").write("\n".join(L) + "\n")
print("moved", np.linalg.norm(x[:3] - p0), "rot deg", np.degrees(np.linalg.norm(x[3:])))
