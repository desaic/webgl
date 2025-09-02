
import { Euler, Vector3, Matrix4 } from 'three';
import { CuboidConf } from './CuboidConf';

export class RoundedCuboid {
    //scale.
    public alpha: number;
    //postion of center when cuboid is axis aligned.
    public position: Vector3;
    public rotation: Matrix4;
    public sphereRadius: number;
    public size: Vector3;
    // configuration from ui
    public conf: CuboidConf;
    public id: number;
    constructor() {
        this.alpha = 1.0;
        this.position = new Vector3(0,0,0);
        //identity
        this.rotation = new Matrix4();
        this.size = new Vector3(200,100,50);
        this.sphereRadius = 5;
        this.conf = new CuboidConf();
        this.id = -1;
    }
    // copy configuration into cuboid's state
    UpdateFromConf(){
        this.position.x = this.conf.x
        this.position.y = this.conf.y
        this.position.z = this.conf.z
        const e = new Euler(this.conf.rx, this.conf.ry, this.conf.rz);
        this.rotation.makeRotationFromEuler(e);
    }

    MinSideLength(){
        return Math.min(this.size.x, this.size.y, this.size.z);
    }
}