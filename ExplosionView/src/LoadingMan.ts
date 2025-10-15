import { STLLoader } from "./STLLoader.js";
import { OBJLoader } from "./OBJLoader.ts";
import { Mesh } from "three";
import {FileStream} from "./FileStream.js"
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

  async readChunkAsText(chunkBlob) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = (event) => resolve(event.target.result);
      reader.onerror = (event) => reject(event.target.error);
      reader.readAsText(chunkBlob);
    });
  }

  async splitHugeFileLines(file) {
    const CHUNK_SIZE = 1024 * 1024; // 1MB chunks
    let all_lines = [];
    const numChunks = Math.ceil(file.size / CHUNK_SIZE);
    let lineBuffer = "";

    // 2. Use a for loop to iterate over the chunks
    for (let i = 0; i < numChunks; i++) {
      const start = i * CHUNK_SIZE;
      const end = Math.min(start + CHUNK_SIZE, file.size);
      const chunk = file.slice(start, end);

      const text = await this.readChunkAsText(chunk);

      lineBuffer += text;
      const lines = lineBuffer.split("\n");

      const lastLine = lines.pop();

      all_lines.push(...lines)
      
      lineBuffer = lastLine;
    }

    // Process any remaining content
    if (lineBuffer) {
      all_lines.push(lineBuffer)
    }
    return all_lines;
  }

  async ReadOBJ(file: File) {
    try {
      const ten_megs = 1024 * 10240;
      const fs = new FileStream(file, ten_megs);
      var loader = new OBJLoader(this, file.name);
      const object = await loader.parse_lines(fs);
      if (object.isGroup) {
        console.log('loaded no. meshes ' + object.children.length);
        for (let i = 0; i < object.children.length; i++) {
          const child = object.children[i];
          const mesh = child as Mesh;
          if(mesh && mesh.geometry){
            mesh.geometry.computeVertexNormals();
          }
          this.onLoad(child.name, child);
          if (i % 20 == 0) {
            await new Promise((resolve) => setTimeout(resolve, 0));
          }
        }
      } else {
      }
      console.log("copy to gpu done");
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
