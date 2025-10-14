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

  readHugeFileInLines (file, onComplete) {
    const CHUNK_SIZE = 1024 * 1024; // 1MB chunks
    let offset = 0;
    let buffer = '';
    let all_lines = []
    function readNextChunk() {
        if (offset >= file.size) {
            // Process any remaining data in the buffer as the last line
            if (buffer.length > 0) {
              all_lines.push(buffer);
            }
            onComplete(all_lines);
            return;
        }

        const slice = file.slice(offset, offset + CHUNK_SIZE);
        const reader = new FileReader();

        reader.onload = function(e) {
            buffer += e.target.result;
            const lines = buffer.split(/\r?\n/);

            // Process all complete lines
            for (let i = 0; i < lines.length - 1; i++) {
                all_lines.push(lines[i]);
            }

            // Store the last part as a potential incomplete line
            buffer = lines[lines.length - 1];

            offset += CHUNK_SIZE;
            readNextChunk(); // Read the next chunk
        };

        reader.readAsText(slice);
    }
    readNextChunk(); // Start reading    
  }

  ReadOBJ(file: File) {
    try {        
      const onComplete = (all_lines) =>{
        var loader = new OBJLoader(this, file.name);
        const object = loader.parse_lines(all_lines);
        if (object.isGroup) {
          for (const child of object.children) {
            this.onLoad(child.name, child);
          }
        } else {
        }
        console.log("copy to gpu done");   
      }
      this.readHugeFileInLines(file, onComplete);
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
