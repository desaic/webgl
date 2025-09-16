import { RoundedCuboid } from './RoundedCuboid'
import {Vector3, Matrix4} from "three"
//solver variables for two cuboids
export class SolverVars {
  // scaling
  alpha: number;
  // intersection point
  x: number[];
  // slack variables in cuboids 1 and 2.
  // in local coordinates of respective cuboids
  y1: number[];
  y2: number[];
  // lagrange multiplier for ||x - R1y1||<=alpha * R1    
  l1R: number;
  l2R: number;
  // lagrange multiplier for y<= alpha * di
  // ordered in +xyz and -xyz
  l1d: number[];
  l2d: number[];
  // inverse barrier scaling for interior point method.
  // must > 0.
  t: number;
  constructor() {
    this.alpha = 1.0;
    this.x = new Array(3).fill(0);
    this.y1 = new Array(3).fill(0);
    this.y2 = new Array(3).fill(0);
    this.l1R = 0;
    this.l1R = 0;
    this.l1d = new Array(6).fill(0);
    this.l2d = new Array(6).fill(0);
    this.t = 1;
  }
};

// ||x||^2
function Norm2 (x:number[]){
  let sum = 0;
  for(let i = 0;i<x.length;i++){
    sum += x[i] * x[i];
  }
  return sum;
}

// a= a-b.
function Sub(a:number[], b:number[]){
  for(let i = 0;i<a.length;i++){
    a[i] -= b[i];
  }
}

function AddArr(a:number[], b:number[]){
  for(let i = 0;i<a.length;i++){
    a[i] += b[i];
  }
}

function AddVector3(a:number[], b:Vector3){  
  a[0] += b.x;
  a[1] += b.y;
  a[2] += b.z;
}

// ignores translation 
function MVProd(M: Matrix4, x:number[]){
  const y = [0,0,0];
  for(let row = 0; row<3;row++){
    for(let col = 0;col<3;col++){
      //threejs is column major
      y[row] += M.elements[col * 4 + row] *x[col];
    }    
  }
  return y;
}

export class InteriorPtCuboid{
  static NUM_CONSTRAINTS = 14;
  static NUM_VARS = 10;
  // original objective, which is just minimizing alpha. not useful.
  // here for completeness.
  static F_Orig(vars: SolverVars, cuboids: RoundedCuboid[]): number {
    return vars.alpha;
  }
  // interior point objective. includes log barriers for all constraints. returns 
  // infinity if infeasible.
  static F_IP(vars: SolverVars, cuboids: RoundedCuboid[]): number {
    let val = vars.alpha;
    for(let i = 0;i<this.NUM_CONSTRAINTS;i++){

    }
    return val;
  }
  // derivative with interior point purturbed kkt
  static DF_IP(vars: SolverVars, cuboids: RoundedCuboid[]): number {
    const df = new Array(InteriorPtCuboid.NUM_VARS);

    return df;
  }
  static C(vars: SolverVars, cuboids: RoundedCuboid[]): number[] {
    const cvals = new Array(InteriorPtCuboid.NUM_CONSTRAINTS);
    let tmpVec = Array.from(vars.x);
    let yWorld = Array.from(vars.y1);
    yWorld = MVProd(cuboids[0].rotation, vars.y1);
    // yWorld = Qy + O
    AddVector3(yWorld, cuboids[0].position);    
    Sub(tmpVec, yWorld);
    let R1sqr=vars.alpha * cuboids[0].sphereRadius;
    R1sqr *= R1sqr;
    
    cvals[0] = Norm2(tmpVec) - R1sqr;
    return cvals;
  }
}
