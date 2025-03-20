export class Array3D {
  constructor(sx,sy,sz) {
    this.size = [sx,sy,sz];
    this.data = new Float32Array(sx*sy*sz);
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
}