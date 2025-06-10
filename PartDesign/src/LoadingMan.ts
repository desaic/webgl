import { Object3D } from "three";
/// a wrapper for callback functions during mesh loading
export class LoadingMan {
    constructor(
        onLoad?: (name: string, obj:Object3D) => void,
        onProgress?: (name: string, loaded: number, total: number) => void,
        onError?: (url: string) => void,
    ){
        this.onLoad = onLoad;
        this.onProgress = onProgress;
        this.onError = onError;
    }

    /**
     * Will be called when an item finished loading
     */
    onLoad: (name: string, obj:Object3D) => void;

    /**
     * Will be called for each loaded item.
     * The default is a function with empty body.
     * @param url The url of the item just loaded.
     * @param loaded The number of items already loaded so far.
     * @param total The total amount of items to be loaded.
     */
    onProgress: (name: string, loaded: number, total: number) => void;

    /**
     * Will be called when item loading fails.
     * The default is a function with empty body.
     * @param url The url of the item that errored.
     */
    onError: (name: string, msg: string) => void;
}