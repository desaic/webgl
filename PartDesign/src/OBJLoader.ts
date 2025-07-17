import {
  BufferGeometry,
  Float32BufferAttribute,
  Group,
  Mesh,
  MeshPhongMaterial
} from "three";
import {LoadingMan} from "./LoadingMan.ts"
// o object_name | g group_name
const _object_pattern = /^[og]\s*(.+)?/;
const _face_vertex_data_separator_pattern = /\s+/;

const defaultObjMaterial = new MeshPhongMaterial({color:0x808080, specular: 0x808080});
function ParserState(baseName) {
  const state = {
    objects: [],
    object: {},

    vertices: [],
    uvs: [],

    startObject: function (name, fromDeclaration) {
      // If the current object (initial from reset) is not from a g/o declaration in the parsed
      // file. We need to use it for the first parsed g/o to keep things in sync.
      if (this.object && this.object.fromDeclaration === false) {
        this.object.name = name;
        this.object.fromDeclaration = fromDeclaration !== false;
        return;
      }

      this.object = {
        name: name || "",
        fromDeclaration: fromDeclaration !== false,

        geometry: {
          vertices: [],
          uvs: [],
          hasUVIndices: false,
        },

      };

      this.objects.push(this.object);
    },

    parseVertexIndex: function (value, len) {
      const index = parseInt(value, 10);
      return (index >= 0 ? index - 1 : index + len / 3) * 3;
    },

    parseNormalIndex: function (value, len) {
      const index = parseInt(value, 10);
      return (index >= 0 ? index - 1 : index + len / 3) * 3;
    },

    parseUVIndex: function (value, len) {
      const index = parseInt(value, 10);
      return (index >= 0 ? index - 1 : index + len / 2) * 2;
    },

    addVertex: function (a, b, c) {
      const src = this.vertices;
      const dst = this.object.geometry.vertices;

      dst.push(src[a + 0], src[a + 1], src[a + 2]);
      dst.push(src[b + 0], src[b + 1], src[b + 2]);
      dst.push(src[c + 0], src[c + 1], src[c + 2]);
    },

    addVertexPoint: function (a) {
      const src = this.vertices;
      const dst = this.object.geometry.vertices;

      dst.push(src[a + 0], src[a + 1], src[a + 2]);
    },

    addUV: function (a, b, c) {
      const src = this.uvs;
      const dst = this.object.geometry.uvs;

      dst.push(src[a + 0], src[a + 1]);
      dst.push(src[b + 0], src[b + 1]);
      dst.push(src[c + 0], src[c + 1]);
    },

    addDefaultUV: function () {
      const dst = this.object.geometry.uvs;

      dst.push(0, 0);
      dst.push(0, 0);
      dst.push(0, 0);
    },

    addFace: function (a, b, c, ua, ub, uc) {
      const vLen = this.vertices.length;

      let ia = this.parseVertexIndex(a, vLen);
      let ib = this.parseVertexIndex(b, vLen);
      let ic = this.parseVertexIndex(c, vLen);

      this.addVertex(ia, ib, ic);

      // uvs

      if (ua !== undefined && ua !== "") {
        const uvLen = this.uvs.length;

        ia = this.parseUVIndex(ua, uvLen);
        ib = this.parseUVIndex(ub, uvLen);
        ic = this.parseUVIndex(uc, uvLen);

        this.addUV(ia, ib, ic);

        this.object.geometry.hasUVIndices = true;
      } else {
        // add placeholder values (for inconsistent face definitions)

        this.addDefaultUV();
      }
    },
  };

  state.startObject(baseName, false);

  return state;
}

//

class OBJLoader {
  constructor(man: LoadingMan, baseName:string) {
    this.manager = man;
    this.baseName = baseName;
  }
  manager: LoadingMan;
  baseName: string;
  parse(text) {
    const state = ParserState(this.baseName);

    if (text.indexOf("\r\n") !== -1) {
      // This is faster than String.split with regex that splits on both
      text = text.replace(/\r\n/g, "\n");
    }

    if (text.indexOf("\\\n") !== -1) {
      // join lines separated by a line continuation character (\)
      text = text.replace(/\\\n/g, "");
    }

    const lines = text.split("\n");
    let result = [];
    const reportFreq = 100000;
    for (let i = 0; i < lines.length; i++) {
      if (i % reportFreq == 0) {
        this.manager.onProgress(this.baseName, "parse", i, lines.length);
      }
      const line = lines[i].trimStart();

      if (line.length === 0) continue;

      const lineFirstChar = line.charAt(0);

      // @todo invoke passed in handler if any
      if (lineFirstChar === "#") continue; // skip comments

      if (lineFirstChar === "v") {
        const data = line.split(_face_vertex_data_separator_pattern);

        switch (data[0]) {
          case "v":
            state.vertices.push(
              parseFloat(data[1]),
              parseFloat(data[2]),
              parseFloat(data[3])
            );
            break;
          case "vt":
            state.uvs.push(parseFloat(data[1]), parseFloat(data[2]));
            break;
        }
      } else if (lineFirstChar === "f") {
        const lineData = line.slice(1).trim();
        const vertexData = lineData.split(_face_vertex_data_separator_pattern);
        const faceVertices = [];

        // Parse the face vertex data into an easy to work with format

        for (let j = 0, jl = vertexData.length; j < jl; j++) {
          const vertex = vertexData[j];

          if (vertex.length > 0) {
            const vertexParts = vertex.split("/");
            faceVertices.push(vertexParts);
          }
        }

        // Draw an edge between the first vertex and all subsequent vertices to form an n-gon
        // does not work for concave faces.

        const v1 = faceVertices[0];

        for (let j = 1, jl = faceVertices.length - 1; j < jl; j++) {
          const v2 = faceVertices[j];
          const v3 = faceVertices[j + 1];

          state.addFace(v1[0], v2[0], v3[0], v1[1], v2[1], v3[1]);
        }
      } else if ((result = _object_pattern.exec(line)) !== null) {
        // o object_name
        // or
        // g group_name

        // WORKAROUND: https://bugs.chromium.org/p/v8/issues/detail?id=2869
        // let name = result[ 0 ].slice( 1 ).trim();
        const name = (" " + result[0].slice(1).trim()).slice(1);

        state.startObject(this.baseName + "_" + name, true);
      } else {
        // Handle null terminated files without exception
        if (line === "\0") continue;
      }
    }

    const container = new Group();

    const hasPrimitives = !(
      state.objects.length === 1 &&
      state.objects[0].geometry.vertices.length === 0
    );

    if (hasPrimitives === true) {
      for (let i = 0, l = state.objects.length; i < l; i++) {
        const object = state.objects[i];
        const geometry = object.geometry;

        // Skip o/g line declarations that did not follow with any faces
        if (geometry.vertices.length === 0) continue;

        const buffergeometry = new BufferGeometry();

        buffergeometry.setAttribute(
          "position",
          new Float32BufferAttribute(geometry.vertices, 3)
        );

        if (geometry.hasUVIndices === true) {
          buffergeometry.setAttribute(
            "uv",
            new Float32BufferAttribute(geometry.uvs, 2)
          );
        }

        // Create mesh

        let mesh;
        buffergeometry.computeVertexNormals();
        mesh = new Mesh(buffergeometry, defaultObjMaterial);

        mesh.name = object.name;
        
        container.add(mesh);
      }
    }

    return container;
  }
}

export { OBJLoader };
