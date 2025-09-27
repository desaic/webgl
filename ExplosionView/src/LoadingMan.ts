import { STLLoader } from "./STLLoader.js";
import { OBJLoader } from "./OBJLoader.ts";

/// file loading manager
export class LoadingMan {
  constructor(
    onLoad?: (name: string, obj: any) => void,
    onError?: (url: string) => void
  ) {
    this.onLoad = onLoad;
    this.onError = onError;
    this.loaded = 0;
    this.total = 1;
    this.busy = false;
  }
  /**
   * Will be called when an item finished loading
   */
  onLoad: (name: string, obj: any) => void;

  busy: boolean;
  /// @brief param name The name of the item being loaded.
  name: string;
  /// @brief Name of the operation being performed (e.g. uploading, parsing).
  step: string;
  /// @brief The number of items (bytes, triangles, etc.) already loaded so far.
  loaded: number;
  /// @brief The total number of items to be loaded.
  total: number;

  /**
   * Will be called when item loading fails.
   * The default is a function with empty body.
   * @param name The name of the item that errored.
   */
  onError: (name: string, msg: string) => void;

  ReadSTL(file: File) {
    const reader = new FileReader();
    try {
      reader.onload = () => {
        const loader = new STLLoader();
        const geometry = loader.parse(reader.result);
        this.onLoad(file.name, geometry);
      };
      reader.readAsArrayBuffer(file);
    } catch (err) {
      console.log(err.message);
    }
  }

  ReadOBJ(file: File) {
    const reader = new FileReader();
    try {        
      reader.onload = () => {
        var loader = new OBJLoader(this, file.name);
        console.log("upload done");
        const object = loader.parse(reader.result);
        console.log("parsing done");
        if (object.isGroup) {
          for (const child of object.children) {
            this.onLoad(child.name, child);
          }
        } else {
        }
        console.log("copy to gpu done");   
      };
      reader.readAsText(file);
    } catch (err) {
      console.log(err.message);
    }
  }

  /// a single file may contain multiple meshes.
  async LoadMesh(file: File) {
    const extension = file.name.split(".").pop().toLowerCase();
    if (extension === "stl") {
      this.ReadSTL(file);
    } else if (extension === "obj") {
      this.ReadOBJ(file);
    } else {
      console.log(file, "unknown file type");
    }
  }

  async LoadMeshes(files: FileList) {
    const proms = [];
    for (let i = 0; i < files.length; i++) {
      const file = files[i];
      proms.push(this.LoadMesh(file));
    }
    await Promise.all(proms);
    this.busy = false;    
  }

  SetBusy(b: boolean) {
    this.busy = b;
  }
}
