import * as THREE from "three";

export class GridMesh {

  // quads for drawing texture image
  public mesh: THREE.Mesh;
  // rows then cols
  private size: number[];

  constructor(size_x, size_y) {
    this.size = [size_x, size_y];
    this.resize(size_x, size_y);
  }

  public resize(size_x: number, size_y: number, dx: number = 1) {
    const numSquares = size_x * size_y;
    const numVerts = 4 * numSquares;
    const p = new Float32Array(3 * numVerts);
    const uv = new Float32Array(2 * numVerts);
    const indices=  new Uint32Array(6 * numSquares);

    for (let y = 0; y < size_y; y++) {
      for (let x = 0; x < size_x; x++) {
        // each quad uses 12 floats
        const v0 = this.V0(x, y);
        // 2 3
        // 0 1
        this.SetVertex(p, v0, x * dx, y * dx, 0);
        this.SetVertex(p, v0 + 1, (x + 1) * dx, y * dx, 0);
        this.SetVertex(p, v0 + 2, x * dx, (y + 1) * dx, 0);
        this.SetVertex(p, v0 + 3, (x + 1) * dx, (y + 1) * dx, 0);

        this.SetUV(uv, v0, 0, 0);
        this.SetUV(uv, v0 + 1, 1, 0);
        this.SetUV(uv, v0 + 2, 0, 1);
        this.SetUV(uv, v0 + 3, 1, 1);

        const i0 = this.QuadIndex0(x,y);
        indices[i0] = v0;
        indices[i0 + 1] = v0 + 1;
        indices[i0 + 2] = v0 + 2;
        indices[i0+3] = v0 + 1;
        indices[i0+4] = v0 + 3;
        indices[i0+5] = v0 + 2;
      }
    }
    const geom = new THREE.BufferGeometry()
    geom.setAttribute("position", new THREE.BufferAttribute(p, 3));
    geom.setAttribute("uv", new THREE.BufferAttribute(uv, 2));
    geom.setIndex(new THREE.BufferAttribute(indices,1));
    this.mesh = new THREE.Mesh(geom);
  }


  public SetVertex(p: Float32Array, vIdx: number, x: number, y: number, z: number) {
    p[3 * vIdx] = x;
    p[3 * vIdx + 1] = y;
    p[3 * vIdx + 2] = z;
  }

  public SetUV(uv: Float32Array, vIdx: number, u: number, v: number) {
    uv[2 * vIdx] = u;
    uv[2 * vIdx + 1] = v;
  }

  //first vertex index for quad (x,y)
  public V0(x: number, y: number) {
    return 4 * (x + y * this.size[0]);
  }

  public QuadIndex0(x:number, y : number){
    return 6 * (x + y * this.size[0]);
  }
  public dispose() {
    this.mesh.geometry.dispose();
  }
}