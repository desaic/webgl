import { Array3D } from "./Array3D.js";
import { Vec3f } from "./Vec3f.js";

/// point line segment distance
function PtLineSegDist( p, l0, l1) {
    const l = l1.subtract(l0);
    const l0p = p.subtract(l0);
    if (l0p.dot(l) <= 0.0 || l.norm() < 1e-6) {
      return l0p.norm();
    }
    const l1p = p.subtract(l1);
    if (l1p.dot(l) >= 0.0) {
      return l1p.norm();
    }
    const c = l.cross(l0p);
    return c.norm() / l.norm();
  }
  
  /// draw distance field for a beam on top of existing grid.
  ///  very inefficient. called only during init.
  export function DrawBeamDist(p0, p1, grid, voxelSize) {
    const s = grid.size;
    const p = new Vec3f();
    for (let x = 0; x < s[0]; x++) {
      for (let y = 0; y < s[1]; y++) {
        for (let z = 0; z < s[2]; z++) {
          p.Set(x,y,z);
          const pmm = p.Mult(voxelSize);
          const d = PtLineSegDist(pmm, p0, p1);
          if (d < grid.Get(x, y, z)) {
            grid.Set(x, y, z, d);
          }
        }
      }
    }
  }

  export function DrawBeams(numBeams, beams, grid, voxelSize, cellSize) {
    for (let b = 0; b < numBeams; b++) {
      const p0 = new Vec3f(beams[b][0], beams[b][1], beams[b][2]);
      const p1 = new Vec3f(beams[b][3], beams[b][4], beams[b][5]);
      const p0mm = p0.Mult(cellSize);
      const p1mm = p1.Mult(cellSize);
      DrawBeamDist(p0mm, p1mm, grid, voxelSize);
    }
  }

  export function MakeDiamondCell(voxelSize, cellSize, grid) {
    grid.data.fill(10000);
    const NUM_BEAMS = 28;
    // 16 beams within the cell and 12 beams for beams outside of the cell.
    const beams = [
      [0.25, 0.25, 0.25, 0, 0, 0],
      [0.25, 0.25, 0.25, 0.5, 0.5, 0],
      [0.25, 0.25, 0.25, 0.5, 0, 0.5],
      [0.25, 0.25, 0.25, 0, 0.5, 0.5],

      [0.75, 0.75, 0.25, 1, 1, 0],
      [0.75, 0.75, 0.25, 0.5, 0.5, 0],
      [0.75, 0.75, 0.25, 1, 0.5, 0.5],
      [0.75, 0.75, 0.25, 0.5, 1, 0.5],

      [0.25, 0.75, 0.75, 0.5, 0.5, 1],
      [0.25, 0.75, 0.75, 0, 1, 1],
      [0.25, 0.75, 0.75, 0, 0.5, 0.5],
      [0.25, 0.75, 0.75, 0.5, 1, 0.5],

      [0.75, 0.25, 0.75, 1, 0, 1],
      [0.75, 0.25, 0.75, 0.5, 0, 0.5],
      [0.75, 0.25, 0.75, 1, 0.5, 0.5],
      [0.75, 0.25, 0.75, 0.5, 0.5, 1],

      [1, 0, 0, 1.25, 0.25, 0.25],
      [1, 0, 0, 0.75, -0.25, 0.25],
      [1, 0, 0, 0.75, 0.25, -0.25],

      [0, 1, 0, -0.25, 0.75, 0.25],
      [0, 1, 0, 0.25, 1.25, 0.25],
      [0, 1, 0, 0.25, 0.75, -0.25],

      [0, 0, 1, -0.25, 0.25, 0.75],
      [0, 0, 1, 0.25, -0.25, 0.75],
      [0, 0, 1, 0.25, 0.25, 1.25],

      [1, 1, 1, 1.25, 0.75, 0.75],
      [1, 1, 1, 0.75, 1.25, 0.75],
      [1, 1, 1, 0.75, 0.75, 1.25],
    ];
  
    DrawBeams(NUM_BEAMS, beams, grid, voxelSize, cellSize);
    return grid;
  }