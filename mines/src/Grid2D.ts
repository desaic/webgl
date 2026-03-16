    
interface GridSize {
    x: number;
    y: number;
};

export class Grid2D {
    private data: Int32Array;
    private sizeY: number;
    private sizeX: number;


    constructor(sx: number, sy: number, initialValue: number = 0) {
        this.sizeY = sy;
        this.sizeX = sx;
        this.data = new Int32Array(sx * sy).fill(initialValue);
    }

    get(x: number, y: number): number {
        return this.data[y * this.sizeX + x];
    }

    getSize(): GridSize { return { x: this.sizeX, y: this.sizeY }; }
    
    set(x: number, y: number, value: number): void {
        this.data[y * this.sizeX + x] = value;
    }

    allocate(sx: number, sy: number): void {
        const newData = new Int32Array(sx * sy);
        this.data = newData;
        this.sizeY = sy;
        this.sizeX = sx;
    }
}