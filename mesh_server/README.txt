Mesh Server Protocol

2 kinds of message.
Mesh client sends commands to mesh server.
Mesh server sends events to mesh client.

Command structure
| Command name: 2 bytes unsigned | Args and data command specific.

  Commands:
  MESH = 1
    Create a new mesh. If a mesh with the same ID exists, the old mesh is overwritten.
	Args: |mesh ID 2 bytes unsigned | number of triangles 4 bytes unsigned | list of trianges, each represented by 9 floats with 36 bytes
	
  TRIGS
    Args same as MESH's
	Args: |mesh ID 2 bytes unsigned | number of triangles 4 bytes unsigned | list of trianges, each represented by 9 floats with 36 bytes
    updates the vertex positions of mesh. The number of vertices must remain the same as the original mesh. Three.js does not like resizing.
	
  ATTR
    Set additional mesh attributes.
    Args: | mesh ID 2 bytes unsigned | ATTR_NAME 2 bytes | corresponding payload
	  ATTR_NAME:
	    UV_COORD | number of triangles 4 bytes | list of 2d floats for tex coord for each triangle.
		TEX_IMAGE_ID | image id

  TEXTURE
    Args: |image id 2 bytes | width and height 2 bytes each. huge images not supported. | 3 x width x height bytes for pixels |
  
  TRANS = 5
    Set the affine transformation of a mesh 
	Args: |mesh ID 2 bytes | 4x4 float matrix numbers listed in row major.

  GEt = 6
    Get some member from the scene
	Args: | member name 2 bytes unsigned | additional optional args
	Member names: NUM_MESHES=1, 
Received Info or Events (Unimplemented. subject to change) :
  |Event name: 2 bytes unsigned | Args 
  |NUM_MESHES | 4 bytes of number of meshes
  |MOUSE_MOVE | camera origin and x y coordinates in world units. 5 floats. 20 bytes.
    x y coordinates are calculated at 1 unit in front of the camera.
  |LEFT_DOWN|
  |RIGHT_DOWN|
  |LEFT_UP|
  |RIGHT_UP|
  |LEFT_CLICK|
  |RIGHT_CLICK|
  |Key_Down| keyCode
  