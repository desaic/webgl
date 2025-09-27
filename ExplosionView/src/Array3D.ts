export class Array3D {
  public size: [number, number, number];
  public data: Float32Array;
  constructor(sx, sy, sz) {
    this.size = [sx, sy, sz];
    this.data = new Float32Array(sx * sy * sz);
  }

  // Convert 3D coordinates (x, y, z) to a flat index
  _getIndex(x, y, z) {
    return x + this.size[0] * (y + this.size[1] * z);
  }

  // Get the value at (x, y, z)
  Get(x, y, z) {
    const index = this._getIndex(x, y, z);
    return this.data[index];
  }

  // Set the value at (x, y, z)
  Set(x, y, z, value) {
    const index = this._getIndex(x, y, z);
    this.data[index] = value;
  }

  TrilinearInterp(local, defaultVal = 0) {
    let ix = Math.floor(local.x);
    let iy = Math.floor(local.y);
    let iz = Math.floor(local.z);

    if (ix >= this.size[0] || iy >= this.size[1] || iz >= this.size[2]) {
      return defaultVal;
    }
    if (ix == this.size[0] - 1) {
      ix--;
    }
    if (iy == this.size[1] - 1) {
      iy--;
    }
    if (iz == this.size[2] - 1) {
      iz--;
    }
    const a = ix + 1 - local.x;
    const b = iy + 1 - local.y;
    const c = iz + 1 - local.z;
    // v indexed by y and z
    const v00 = a * this.Get(ix, iy, iz) + (1 - a) * this.Get(ix + 1, iy, iz);
    const v10 = a * this.Get(ix, iy + 1, iz) + (1 - a) * this.Get(ix + 1, iy + 1, iz);
    const v01 = a * this.Get(ix, iy, iz + 1) + (1 - a) * this.Get(ix + 1, iy, iz + 1);
    const v11 =
      a * this.Get(ix, iy + 1, iz + 1) + (1 - a) * this.Get(ix + 1, iy + 1, iz + 1);
    const v0 = b * v00 + (1 - b) * v10;
    const v1 = b * v01 + (1 - b) * v11;
    const v = c * v0 + (1 - c) * v1;
    return v;
  }
}
