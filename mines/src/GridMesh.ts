import * as THREE from "three";

export class GridMesh {
 
  // quads for drawing texture image
  public mesh: THREE.Mesh;
  // rows then cols
  private gridSize: number[];  

  constructor(size_x, size_y) {
    this.gridSize = [size_x, size_y];
    this.resize(size_x, size_y);
  }

  public resize(size_x:number, size_y:number){    
    const numSquares = size_x * size_y;
    const numVerts = 4 * numSquares;    
    const p = new Float32Array(3 * numVerts)

    for(let y = 0;y<size_y;y++){
      for(let x = 0;x<size_x;x++){
        // each quad uses 12 floats
        const i0 = 12 * (y * size_x + x);
        
      }
    }
  }

  

  public dispose(){
    this.mesh.geometry.dispose();
  }
}