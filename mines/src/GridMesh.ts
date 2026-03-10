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
    const p = new Float32Array(3 * numVerts);
    const uv = new Float32Array(2 * numVerts);
    for(let y = 0;y<size_y;y++){
      for(let x = 0;x<size_x;x++){
        // each quad uses 12 floats
        const i0 = 12 * (y * size_x + x);
        
      }
    }
  }

  //first vertex index for quad (x,y)
  public V0(x:number, y:number){
    return 4 * (x + y )
  }
  

  public dispose(){
    this.mesh.geometry.dispose();
  }
}