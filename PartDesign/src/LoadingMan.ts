/// a wrapper for callback functions during mesh loading
export class LoadingMan {
    constructor(        
        onLoad?: (name: string, obj: any) => void,
        onProgress?: (name: string, step: string, loaded: number, total: number) => void,
        onError?: (url: string) => void,
    ){
        this.onLoad = onLoad;
        this.onProgress = onProgress;
        this.onError = onError;

    }
    /**
     * Will be called when an item finished loading
     */
    onLoad: (name: string, obj: any) => void;

    /**
     * Called periodically when reading data.
     * The default is a function with empty body.
     * @param name The name of the item being loaded.
     * @param step Name of the operation being performed (e.g. uploading, parsing).
     * @param loaded The number of bytes or other measures already loaded so far.
     * @param total The total amount of bytes to be loaded.
     */
    onProgress: (name: string, step: string, loaded: number, total: number) => void;

    /**
     * Will be called when item loading fails.
     * The default is a function with empty body.
     * @param name The name of the item that errored.
     */
    onError: (name: string, msg: string) => void;
}