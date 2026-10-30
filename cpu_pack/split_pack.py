"""Writes a pack file without the fruit that have a trajectory.

usage: python -I split_pack.py PACK TRAJ OUT

PACK is the full pack the trajectories belong to (for example pack_inner_nudged.txt).
TRAJ is the trajectory file ("Instance N Mesh path" blocks ending in a Final pose).
OUT gets every PACK line except the ones with a trajectory, so OUT plus the Final poses
of TRAJ is the whole pack. The pack line of instance N is found by matching the Final
pose, which also checks the instance numbering.
"""
import sys


def pack_pos(line):
    t = line.split()
    i = t.index("pos")
    return [float(v) for v in t[i + 1:i + 4]]


def read_traj_finals(path):
    finals = {}
    cur = None
    with open(path) as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] == "Instance":
                cur = int(t[1])
            elif t[0] == "Final" and cur is not None:
                i = t.index("pos")
                finals[cur] = [float(v) for v in t[i + 1:i + 4]]
    return finals


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 1
    pack_file, traj_file, out_file = sys.argv[1:4]
    with open(pack_file) as f:
        lines = [l for l in f.read().split("\n")]
    while lines and not lines[-1].strip():
        lines.pop()
    finals = read_traj_finals(traj_file)
    if not finals:
        print("no trajectories in", traj_file)
        return 1

    def matches(offset):
        ok = 0
        for inst, pos in finals.items():
            idx = inst + offset
            if idx < 0 or idx >= len(lines):
                continue
            p = pack_pos(lines[idx])
            if max(abs(p[k] - pos[k]) for k in range(3)) < 1e-2:
                ok += 1
        return ok

    best = max((-1, 0, 1), key=matches)
    ok = matches(best)
    print("instance index offset", best, "- final poses matched:", ok, "of", len(finals))
    if ok != len(finals):
        print("some trajectories do not match their pack line, not writing", out_file)
        return 1
    removed = {inst + best for inst in finals}
    kept = [l for i, l in enumerate(lines) if i not in removed]
    # the first line of a pack file is the fruit count.
    if kept and kept[0].strip().isdigit():
        kept[0] = str(len(kept) - 1)
    with open(out_file, "w") as f:
        f.write("\n".join(kept) + "\n")
    print("pack lines", len(lines), "removed", len(removed), "kept", len(kept), "trajectories", len(finals))
    print("wrote", out_file)
    return 0


if __name__ == "__main__":
    sys.exit(main())
