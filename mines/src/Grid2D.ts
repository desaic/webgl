export class Grid2D {
    private data: Int32Array;
    private rows: number;
    private cols: number;

    constructor(rows: number, cols: number, initialValue: number = 0) {
        this.rows = rows;
        this.cols = cols;
        this.data = new Int32Array(rows * cols).fill(initialValue);
    }

    // Accessor
    get(r: number, c: number): number {
        return this.data[r * this.cols + c];
    }

    // Mutator
    set(r: number, c: number, value: number): void {
        this.data[r * this.cols + c] = value;
    }

    /**
     * Resizes the grid. 
     * Note: Data is preserved where indices overlap.
     */
    resize(newRows: number, newCols: number): void {
        const newData = new Int32Array(newRows * newCols);
        
        // Copy existing data into the new grid
        const minRows = Math.min(this.rows, newRows);
        const minCols = Math.min(this.cols, newCols);

        for (let r = 0; r < minRows; r++) {
            for (let c = 0; c < minCols; c++) {
                newData[r * newCols + c] = this.data[r * this.cols + c];
            }
        }

        this.data = newData;
        this.rows = newRows;
        this.cols = newCols;
    }
}