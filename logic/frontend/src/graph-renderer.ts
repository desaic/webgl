import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import type { ProofGraph } from './types';

const KIND_COLORS: Record<string, number> = {
  axiom: 0x3b82f6,
  theorem: 0x22c55e,
  derived: 0xf59e0b,
};

export class GraphRenderer {
  private scene: THREE.Scene;
  private camera: THREE.PerspectiveCamera;
  private renderer: THREE.WebGLRenderer;
  private controls: OrbitControls;
  private group: THREE.Group;
  private animationId = 0;

  constructor(container: HTMLElement) {
    this.scene = new THREE.Scene();

    this.camera = new THREE.PerspectiveCamera(
      45, container.clientWidth / container.clientHeight, 0.1, 1000,
    );
    this.camera.position.set(5, 4, 8);

    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(container.clientWidth, container.clientHeight);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    container.appendChild(this.renderer.domElement);

    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;

    this.group = new THREE.Group();
    this.scene.add(this.group);

    const light = new THREE.DirectionalLight(0xffffff, 1);
    light.position.set(2, 5, 3);
    this.scene.add(light);
    this.scene.add(new THREE.AmbientLight(0x404040));

    window.addEventListener('resize', this.onResize);
    this.animate();
  }

  loadGraph(graph: ProofGraph) {
    this.group.clear();
    const nodeMap = new Map<number, THREE.Mesh>();

    const radius = 2;
    const n = graph.nodes.length;
    for (let i = 0; i < n; i++) {
      const angle = (i / Math.max(n, 1)) * Math.PI * 2;
      const x = Math.cos(angle) * radius;
      const y = 0;
      const z = Math.sin(angle) * radius;

      const color = KIND_COLORS[graph.nodes[i].kind] ?? 0x888888;
      const sphere = new THREE.Mesh(
        new THREE.SphereGeometry(0.15, 32, 16),
        new THREE.MeshStandardMaterial({ color }),
      );
      sphere.position.set(x, y, z);
      sphere.userData = graph.nodes[i];
      this.group.add(sphere);
      nodeMap.set(graph.nodes[i].id, sphere);
    }

    for (const edge of graph.edges) {
      const fromId = Array.isArray(edge.from) ? edge.from[0] : edge.from;
      const from = nodeMap.get(fromId);
      const to = nodeMap.get(edge.to);
      if (!from || !to) continue;

      const dir = to.position.clone().sub(from.position);
      const mid = from.position.clone().add(dir.clone().multiplyScalar(0.5));
      const length = dir.length();
      dir.normalize();

      const cyl = new THREE.Mesh(
        new THREE.CylinderGeometry(0.03, 0.03, length, 8),
        new THREE.MeshStandardMaterial({ color: 0x666666 }),
      );
      cyl.position.copy(mid);
      cyl.quaternion.setFromUnitVectors(
        new THREE.Vector3(0, 1, 0),
        dir,
      );
      this.group.add(cyl);
    }
  }

  resetCamera() {
    this.camera.position.set(5, 4, 8);
    this.controls.target.set(0, 0, 0);
    this.controls.update();
  }

  private onResize = () => {
    const el = this.renderer.domElement.parentElement!;
    this.camera.aspect = el.clientWidth / el.clientHeight;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(el.clientWidth, el.clientHeight);
  };

  private animate = () => {
    this.animationId = requestAnimationFrame(this.animate);
    this.controls.update();
    this.renderer.render(this.scene, this.camera);
  };

  dispose() {
    cancelAnimationFrame(this.animationId);
    window.removeEventListener('resize', this.onResize);
    this.controls.dispose();
    this.renderer.dispose();
  }
}
