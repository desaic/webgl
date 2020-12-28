Mesh Server Protocol

2 kinds of message.
Mesh client sends commands to mesh server.
Mesh server sends events to mesh client.

Command structure
| Command name: 2 bytes unsigned | Args and data command specific.

  Commands:
  MESH
    Create a new mesh. If a mesh with the same ID exists, the old mesh is overwritten.
	Args: |mesh ID 2 bytes unsigned | number of triangles 4 bytes unsigned | list of trianges, each represented by 9 floats with 36 bytes
  UPDATE_TRIGS
    Args same as MESH's
	Args: |mesh ID 2 bytes unsigned | number of triangles 4 bytes unsigned | list of trianges, each represented by 9 floats with 36 bytes
    updates the vertex positions of mesh. The number of vertices must remain the same as the original mesh. Three.js does not like resizing.
  TRANS
    Set the affine transformation of a mesh 
	Args: |mesh ID 2 bytes | 4x4 float matrix numbers listed in row major.

  
Received Events (Unimplemented) :
  |Event name: 2 bytes unsigned | Args 
  |MOUSE_MOVE | camera origin and x y coordinates in world units. 5 floats. 20 bytes.
    x y coordinates are calculated at 1 unit in front of the camera.
  |LEFT_DOWN|
  |RIGHT_DOWN|
  |LEFT_UP|
  |RIGHT_UP|
  |LEFT_CLICK|
  |RIGHT_CLICK|
  |Key_Down| keyCode
  