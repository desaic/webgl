export class Grid2D {
    private data: Int32Array;
    private sizeY: number;
    private sizeX: number;

    constructor(sy: number, sx: number, initialValue: number = 0) {
        this.sizeY = sy;
        this.sizeX = sx;
        this.data = new Int32Array(sx * sy).fill(initialValue);
    }

    get(r: number, c: number): number {
        return this.data[r * this.sizeX + c];
    }

    set(r: number, c: number, value: number): void {
        this.data[r * this.sizeX + c] = value;
    }

    resize(newRows: number, newCols: number): void {
        const newData = new Int32Array(newRows * newCols);
        
        // Copy existing data into the new grid
        const minRows = Math.min(this.sizeY, newRows);
        const minCols = Math.min(this.sizeX, newCols);

        for (let r = 0; r < minRows; r++) {
            for (let c = 0; c < minCols; c++) {
                newData[r * newCols + c] = this.data[r * this.sizeX + c];
            }
        }

        this.data = newData;
        this.sizeY = newRows;
        this.sizeX = newCols;
    }
}