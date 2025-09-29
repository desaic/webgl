export class Vec3f {
  public x: number;
  public y: number;
  public z: number;
  constructor(x = 0, y = 0, z = 0) {
    this.x = x;
    this.y = y;
    this.z = z;
  }

  // Bracket operator to access members by index
  get(index) {
    if (index === 0) return this.x;
    if (index === 1) return this.y;
    if (index === 2) return this.z;
  }

  Set(x, y, z) {
    this.x = x;
    this.y = y;
    this.z = z;
  }

  // Bracket operator to set members by index
  setAt(index, value) {
    if (index === 0) this.x = value;
    else if (index === 1) this.y = value;
    else if (index === 2) this.z = value;
  }

  add(other) {
    return new Vec3f(this.x + other.x, this.y + other.y, this.z + other.z);
  }

  Mult(other) {
    return new Vec3f(this.x * other.x, this.y * other.y, this.z * other.z);
  }

  subtract(other) {
    return new Vec3f(this.x - other.x, this.y - other.y, this.z - other.z);
  }

  cross(other) {
    return new Vec3f(
      this.y * other.z - this.z * other.y,
      -this.x * other.z + this.z * other.x,
      this.x * other.y - this.y * other.x
    );
  }

  dot(other) {
    return this.x * other.x + this.y * other.y + this.z * other.z;
  }

  norm2() {
    return this.x * this.x + this.y * this.y + this.z * this.z;
  }

  norm() {
    return Math.sqrt(this.norm2());
  }
}
