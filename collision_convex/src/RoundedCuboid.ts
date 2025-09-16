
import { Euler, Vector3, Matrix4 } from 'three';
import { CuboidConf } from './CuboidConf';

export class RoundedCuboid {
    //scale for rendering.
    public alpha: number;
    //postion of center when cuboid is axis aligned.
    public position: Vector3;
    public rotation: Matrix4;
    // chamfer radius
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
    //@brief copy values from other to this
    Copy(other:RoundedCuboid){
        this.alpha = other.alpha;
        this.position.copy(other.position);        
        this.rotation.copy(other.rotation);
        this.size.copy(other.size);
        this.sphereRadius = other.sphereRadius;        
        this.id = other.id;
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