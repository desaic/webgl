import { Vector3 } from "three";

/**
 * Class representing a single node in the Explosion Graph.
 * It combines hierarchical data (parent/children) with visualization properties (explosion).
 */
export class ExplosionNode {
  // Structural Properties
  public name: string;

  // Hierarchy Properties
  // Note: Parent is stored as a reference to the parent Node or null if it's a root node.
  public parent: ExplosionNode | null;
  public children: ExplosionNode[];

  //ui garbage shouldn't be here
  public isCollapsed: boolean;
  // disassembly order , the opposite of assembly order
  // smaller number goes out first or comes in last.
  public disasOrder: number;
  // Visualization Properties
  // explosion direction relative to parent
  public direction: Vector3;
  // explosion distance relative to parent
  public distance: number; // A numeric value representing the distance to explode, e.g., in world units.
  // global displacement adding all parents.
  public globalDisp: Vector3;
  public selected:boolean;
  public meshCenter:Vector3;
  /**
   * @param name - Display name of the node.
   * @param parent - Optional reference to the parent node.
   */
  constructor(
    name: string,
    parent: ExplosionNode | null = null,
    distance: number = 1.0,
    direction: Vector3 = new Vector3(1, 0, 0)
  ) {
    this.name = name;
    this.parent = parent;
    this.children = [];
    this.distance = distance;
    this.direction = direction;
    this.disasOrder = 0;
    this.globalDisp = new Vector3(0,0,0);
    // Automatically link the new node to its parent's children list
    if (this.parent) {
      this.parent.children.push(this);
    }
    this.isCollapsed=true;
    this.selected = false;
    this.meshCenter = new Vector3(0,0,0);
  }

  /**
   * Helper method to add a child to this node.
   * @param childNode - The child ExplosionNode instance to add.
   */
  public addChild(childNode: ExplosionNode): void {
    // Update the child's parent reference (if not already set)
    if (childNode.parent !== this) {
      childNode.parent = this;
    }
    let dup = false;
    for (let i = 0; i < this.children.length; i++) {
      if (this.children[i] == childNode) {
        dup = true;
        break;
      }
    }

    if (!dup) {
      this.children.push(childNode);
    }
  }

  public removeChild(childNode: ExplosionNode): void {
    // Update the child's parent reference (if not already set)
    if (childNode.parent === this) {
      childNode.parent = null;
    }
    // linear search
    for (let i = 0; i < this.children.length; i++) {
      if (this.children[i] == childNode) {
        this.children.splice(i, 1);
        break;
      }
    }
  }

  public setParent(parentNode: ExplosionNode) {
    if (this.parent == parentNode) {
      return;
    }
    if (this.parent != null) {
      this.parent.removeChild(this);
    }
    this.parent = parentNode;
    if (parentNode != null) {
      parentNode.addChild(this);
    }
  }

  public destroy(){
    if(this.parent ){
      this.parent.removeChild(this);
    }
    this.parent = null;
    for(let i = 0;i<this.children.length;i++){
      this.children[i].parent = null;
    }
    this.children = [];
  }
}

/**
 * Class to manage the overall Explosion Graph structure.
 */
export class ExplosionGraph {
  public nodes: ExplosionNode[];
  private nodeMap: Map<string, ExplosionNode> ;

  public selectedNode: ExplosionNode;
  public needsUpdate: boolean;
  constructor(
  ) {
    this.nodes = [];
    this.nodeMap = new Map();
    this.needsUpdate = false;
    this.selectedNode = null;
  }

  public empty():boolean{
    return this.nodes.length == 0;
  }
  /**
   * Adds a node to the graph and maps it by ID for fast lookup.
   * @param node - The ExplosionNode instance to add.
   */
  public addNode(node: ExplosionNode): void {
    if (!this.nodeMap.has(node.name)) {
      this.nodes.push(node);
      this.nodeMap.set(node.name, node);
    } else {
      console.warn(`Node with ID ${node.name} already exists.`);
    }
  }

  /**
   * Retrieves a node by its ID.
   * @param id - The ID of the node to retrieve.
   * @returns The ExplosionNode or undefined if not found.
   */
  public getNodeByName(name: string): ExplosionNode | undefined {
    return this.nodeMap.get(name);
  }

  /**
   * Gets all root nodes (nodes without a parent).
   * @returns An array of root ExplosionNodes.
   */
  public getRootNodes(): ExplosionNode[] {
    // Filter nodes where the parent is null
    return this.nodes.filter((node) => node.parent === null);
  }

  /**
   * Example method: Reorganizes the entire graph for a visualization based on the explosion settings.
   * (In a real application, this would calculate 3D coordinates based on direction and distance).
   */
  public printConsole(): void {
    console.log("--- Visualizing Explosion Graph ---");
    this.nodes.forEach((node) => {
      const dist = node.distance;
      const direction = node.direction;
      const parentId = node.parent ? node.parent.name : "N/A (Root)";

      console.log(
        ` ${node.name} ` +
          `(Parent: ${parentId}) -> ` +
          `direction: ${direction} ` +
          `distance: ${dist}`
      );
    });
  }
  public destroy(){
    this.nodeMap.clear();
    for(let i = 0;i<this.nodes.length;i++){
      this.nodes[i].destroy();      
    }
    this.nodes = [];
  }
}
