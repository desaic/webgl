import * as THREE from 'three'
import { VolumeRenderShader1 } from './VolumeShader.js'
///volume render using three js
///and the example VolumeShader
export default class VolRender
{
    constructor(renderIn) {
        // this.loadRaw('./benchy.raw', function (mesh){
        //     var m = mesh;
        // });
        this.render = renderIn;
        this.shader = VolumeRenderShader1;
        //color maps
        this.cmtextures = {
            viridis: new THREE.TextureLoader().load( 'textures/cm_viridis.png', this.render ),
            gray: new THREE.TextureLoader().load( 'textures/cm_gray.png', this.render )
        };
        this.volconfig = { clim1: 0, clim2: 1, renderstyle: 'iso', isothreshold: 0.15, colormap: 'gray' };
        this.geometry = new THREE.BoxGeometry( 500,500,500 );
	}

    parseRaw(arr){
        //header
        var dv = new DataView(arr, 0);
        const LE = true; 
        var sizes = [dv.getInt32(0, LE),dv.getInt32(4, LE),dv.getInt32(8, LE)];

        var uint8Arr = new Uint8Array(arr, 12);
        //scale from material IDs 1 2 3 to grayscale values.
        var scaledData = uint8Arr.map(function(x) { return x * 100; });
        const texture = new THREE.DataTexture3D( scaledData, sizes[0],  sizes[1],  sizes[2] );
        texture.format = THREE.RedFormat;
        texture.minFilter = texture.magFilter = THREE.NearestFilter;
        texture.generateMipmaps = false;
        texture.unpackAlignment = 1;

        const uniforms = THREE.UniformsUtils.clone( this.shader.uniforms );

        uniforms[ "u_data" ].value = texture;
        uniforms[ "u_size" ].value.set( sizes[0], sizes[1], sizes[2] );
        uniforms[ "u_clim" ].value.set( this.volconfig.clim1, this.volconfig.clim2 );
        uniforms[ "u_renderstyle" ].value = this.volconfig.renderstyle == 'mip' ? 0 : 1; // 0: MIP, 1: ISO
        uniforms[ "u_renderthreshold" ].value = this.volconfig.isothreshold; // For ISO renderstyle
        uniforms[ "u_cmdata" ].value = this.cmtextures[ this.volconfig.colormap ];

        var material = new THREE.ShaderMaterial( {
            uniforms: uniforms,
            vertexShader: this.shader.vertexShader,
            fragmentShader: this.shader.fragmentShader,
            side: THREE.BackSide // The volume shader uses the backface as its "reference point"
        } );
        this.geometry = new THREE.BoxGeometry( sizes[0], sizes[1], sizes[2] );
        this.geometry.translate( sizes[0]/2, sizes[1]/2, sizes[2]/2 );
        const mesh = new THREE.Mesh( this.geometry, material );
        return mesh;
    }
}