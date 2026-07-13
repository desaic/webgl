import * as THREE from "three";

const FACE_INFO = [
  { label: "+X", color: "#cc3344", type: "posX" },
  { label: "-X", color: "#661122", type: "negX" },
  { label: "+Y", color: "#33cc44", type: "posY" },
  { label: "-Y", color: "#116622", type: "negY" },
  { label: "+Z", color: "#3344cc", type: "posZ" },
  { label: "-Z", color: "#112266", type: "negZ" },
];

function createFaceTexture(label, bgColor) {
  const size = 128;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = bgColor;
  ctx.fillRect(0, 0, size, size);
  ctx.strokeStyle = "#ffffff";
  ctx.lineWidth = 3;
  ctx.strokeRect(2, 2, size - 4, size - 4);
  ctx.fillStyle = "#ffffff";
  ctx.font = "bold 48px Arial";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText(label, size / 2, size / 2);
  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  return texture;
}

function createAxisSprite(label, colorHex) {
  const size = 64;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d");
  ctx.beginPath();
  ctx.arc(size / 2, size / 2, 16, 0, 2 * Math.PI);
  ctx.fillStyle = colorHex;
  ctx.fill();
  ctx.font = "bold 22px Arial";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillStyle = "#000000";
  ctx.fillText(label, size / 2, size / 2 + 1);
  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  return new THREE.SpriteMaterial({ map: texture, toneMapped: false });
}

const AXES = [
  { color: 0xcc3344, rot: new THREE.Euler(0, 0, -Math.PI / 2), label: "X", pos: [1.2, 0, 0], type: "posX" },
  { color: 0x33cc44, rot: new THREE.Euler(0, 0, 0), label: "Y", pos: [0, 1.2, 0], type: "posY" },
  { color: 0x3344cc, rot: new THREE.Euler(Math.PI / 2, 0, 0), label: "Z", pos: [0, 0, 1.2], type: "posZ" },
];

export class ViewCube extends THREE.Object3D {
  constructor(camera, domElement, controls) {
    super();
    this.isViewCube = true;
    this.animating = false;
    this.dim = 120;

    this._camera = camera;
    this._domElement = domElement;
    this._controls = controls;

    const orthoCamera = new THREE.OrthographicCamera(-2, 2, 2, -2, 0, 10);
    orthoCamera.position.set(0, 0, 5);
    this._orthoCamera = orthoCamera;

    this._raycaster = new THREE.Raycaster();
    this._mouse = new THREE.Vector2();
    this._interactiveObjects = [];

    // --- Cube ---
    const cubeSize = 1.6;
    const cubeMaterials = FACE_INFO.map((info) => {
      const tex = createFaceTexture(info.label, info.color);
      return new THREE.MeshBasicMaterial({
        map: tex,
        transparent: true,
        opacity: 0.8,
        toneMapped: false,
      });
    });
    const cube = new THREE.Mesh(
      new THREE.BoxGeometry(cubeSize, cubeSize, cubeSize),
      cubeMaterials
    );
    this.add(cube);
    this._interactiveObjects.push(cube);

    // --- Arrows + sprites ---
    for (const axis of AXES) {
      const mat = new THREE.MeshBasicMaterial({
        color: axis.color,
        toneMapped: false,
      });

      const shaftLen = 1.04;
      const shaft = new THREE.Mesh(
        new THREE.CylinderGeometry(0.035, 0.035, shaftLen, 8),
        mat
      );
      shaft.position.y = shaftLen / 2;

      const head = new THREE.Mesh(
        new THREE.ConeGeometry(0.09, 0.16, 12),
        mat
      );
      head.position.y = shaftLen + 0.08;

      const group = new THREE.Group();
      group.add(shaft);
      group.add(head);
      group.rotation.copy(axis.rot);
      this.add(group);

      const colorHex = "#" + axis.color.toString(16).padStart(6, "0");
      const sprite = new THREE.Sprite(createAxisSprite(axis.label, colorHex));
      sprite.position.set(...axis.pos);
      sprite.scale.set(0.45, 0.45, 1);
      sprite.userData.type = axis.type;
      this.add(sprite);
      this._interactiveObjects.push(sprite);
    }

    // Animation state
    this._targetPosition = new THREE.Vector3();
    this._targetQuaternion = new THREE.Quaternion();
    this._q1 = new THREE.Quaternion();
    this._q2 = new THREE.Quaternion();
    this._viewport = new THREE.Vector4();
    this._radius = 0;
    this._dummy = new THREE.Object3D();
    this._turnRate = 2 * Math.PI;
  }

  render(renderer) {
    this.quaternion.copy(this._camera.quaternion).invert();
    this.updateMatrixWorld();

    const w = this._domElement.offsetWidth;
    const h = this._domElement.offsetHeight;
    const dim = this.dim;
    // Top-right corner (WebGL viewport origin is bottom-left)
    const x = w - dim;
    const y = h - dim;

    renderer.getViewport(this._viewport);
    renderer.setViewport(x, y, dim, dim);
    renderer.autoClear = false;
    renderer.clearDepth();
    renderer.render(this, this._orthoCamera);
    renderer.autoClear = true;
    renderer.setViewport(
      this._viewport.x,
      this._viewport.y,
      this._viewport.z,
      this._viewport.w
    );
  }

  handleClick(event) {
    if (this.animating) return false;

    const rect = this._domElement.getBoundingClientRect();
    const w = rect.width;
    const h = rect.height;
    const dim = this.dim;

    // Widget occupies top-right: [w-dim, w] x [0, dim] (client coords)
    const widgetLeft = rect.left + (w - dim);
    const widgetTop = rect.top;

    if (
      event.clientX < widgetLeft ||
      event.clientX > widgetLeft + dim ||
      event.clientY < widgetTop ||
      event.clientY > widgetTop + dim
    ) {
      return false;
    }

    this._mouse.x = ((event.clientX - widgetLeft) / dim) * 2 - 1;
    this._mouse.y = -((event.clientY - widgetTop) / dim) * 2 + 1;

    this._raycaster.setFromCamera(this._mouse, this._orthoCamera);
    const intersects = this._raycaster.intersectObjects(
      this._interactiveObjects
    );
    if (intersects.length === 0) return false;

    const intersection = intersects[0];
    let type;
    if (intersection.object.isSprite) {
      type = intersection.object.userData.type;
    } else {
      // BoxGeometry: 2 triangles per face group
      const faceIndex = Math.floor(intersection.faceIndex / 2);
      type = FACE_INFO[faceIndex].type;
    }

    this._prepareAnimation(type);
    this.animating = true;
    return true;
  }

  _prepareAnimation(type) {
    const center = this._controls
      ? this._controls.target
      : new THREE.Vector3();
    const dir = new THREE.Vector3();
    const quat = new THREE.Quaternion();

    switch (type) {
      case "posX":
        dir.set(1, 0, 0);
        quat.setFromEuler(new THREE.Euler(0, Math.PI / 2, 0));
        break;
      case "negX":
        dir.set(-1, 0, 0);
        quat.setFromEuler(new THREE.Euler(0, -Math.PI / 2, 0));
        break;
      case "posY":
        dir.set(0, 1, 0);
        quat.setFromEuler(new THREE.Euler(-Math.PI / 2, 0, 0));
        break;
      case "negY":
        dir.set(0, -1, 0);
        quat.setFromEuler(new THREE.Euler(Math.PI / 2, 0, 0));
        break;
      case "posZ":
        dir.set(0, 0, 1);
        quat.setFromEuler(new THREE.Euler());
        break;
      case "negZ":
        dir.set(0, 0, -1);
        quat.setFromEuler(new THREE.Euler(0, Math.PI, 0));
        break;
    }

    this._radius = this._camera.position.distanceTo(center);
    this._targetPosition.copy(dir).multiplyScalar(this._radius).add(center);

    this._dummy.position.copy(center);
    this._dummy.lookAt(this._camera.position);
    this._q1.copy(this._dummy.quaternion);

    this._dummy.lookAt(this._targetPosition);
    this._q2.copy(this._dummy.quaternion);

    this._targetQuaternion.copy(quat);
  }

  update(delta) {
    if (!this.animating) return;

    const step = delta * this._turnRate;
    this._q1.rotateTowards(this._q2, step);

    const center = this._controls
      ? this._controls.target
      : new THREE.Vector3();
    this._camera.position
      .set(0, 0, 1)
      .applyQuaternion(this._q1)
      .multiplyScalar(this._radius)
      .add(center);

    this._camera.quaternion.rotateTowards(this._targetQuaternion, step);

    if (this._q1.angleTo(this._q2) === 0) {
      this.animating = false;
    }
  }

  dispose() {
    this.traverse((obj) => {
      if (obj.geometry) obj.geometry.dispose();
      if (obj.material) {
        const mats = Array.isArray(obj.material) ? obj.material : [obj.material];
        for (const m of mats) {
          if (m.map) m.map.dispose();
          m.dispose();
        }
      }
    });
  }
}
