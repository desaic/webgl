import {
  LineSegments,
  LineBasicMaterial,
  Float32BufferAttribute,
  BufferGeometry,
  Color
} from "three";

class RectGrid extends LineSegments {

	declare public geometry: BufferGeometry;
	declare public material;
	/**
	 * Constructs a new grid helper.
	 *
	 * @param {number} [size=10] - The size of the grid.
	 * @param {number} [divisions=10] - The number of divisions across the grid.
	 * @param {number|Color|string} [color1=0x444444] - The color of the center line.
	 * @param {number|Color|string} [color2=0x888888] - The color of the lines of the grid.
	 */
	constructor( nx = 10, ny = 10, dx = 1, colorVal1 = 0x444444, colorVal2 = 0x888888 ) {

		const color1 = new Color( colorVal1 );
		const color2 = new Color( colorVal2 );

		const centerx = nx / 2;
    const centery = ny / 2;
		const halfx = nx * dx / 2;
    const halfy = ny * dx / 2;

		const vertices = [], colors = [];
    let j = 0;
		for ( let i = 0; i <= nx; i ++) {
      const x=-halfx + i * dx;
			vertices.push( x, 0, - halfy, x, 0, halfy );
			const color = i === centerx ? color1 : color2;
			color.toArray( colors, j ); j += 3;
			color.toArray( colors, j ); j += 3;
		}
    
		for ( let i = 0; i <= ny; i ++) {
      const y=-halfy + i * dx;
			vertices.push( - halfx, 0, y, halfx, 0, y );

			const color = i === centery ? color1 : color2;

			color.toArray( colors, j ); j += 3;
			color.toArray( colors, j ); j += 3;

		}

		const geometry = new BufferGeometry();
		geometry.setAttribute( 'position', new Float32BufferAttribute( vertices, 3 ) );
		geometry.setAttribute( 'color', new Float32BufferAttribute( colors, 3 ) );

		const material = new LineBasicMaterial( { vertexColors: true, toneMapped: false } );

		super( geometry, material );
	}

	/**
	 * Frees the GPU-related resources allocated by this instance. Call this
	 * method whenever this instance is no longer used in your app.
	 */
	dispose() {

		this.geometry.dispose();
		this.material.dispose();

	}

}

export { RectGrid };