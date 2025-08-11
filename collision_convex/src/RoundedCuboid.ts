
import { Vector3, Matrix3 } from 'three';
export default class RoundedCuboid {
    public center: Vector3;
    public rotation: Matrix3;
    public sphereRadius: number;
    public size: Vector3;
    constructor() {
        this.center = new Vector3(0,0,0);
        //identity
        this.rotation = new Matrix3();
        this.size = new Vector3(4,2,1);
        this.sphereRadius = 0.02;
    }
}