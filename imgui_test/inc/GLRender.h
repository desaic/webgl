#pragma once

#include <string>
#include <memory>

#include "Array2D.h"
#include "Camera.h"
#include "GLTex.h"
#include "TrigMesh.h"
#include "Scene.h"

/// <summary>
/// Mesh with additional opengl buffers used by shaders.
/// </summary>
struct GLBuf {
  // vertex array object
  unsigned int vao = 0;
  // determined by number of vertex attributes in vertex shader.
  static const int NUM_BUF = 3;
  // vertex buffer
  std::vector<unsigned int> b;
  bool _needsUpdate = false;

  std::shared_ptr<TrigMesh> mesh;
  size_t _numTrigs = 0;
  GLTex _tex;
  GLBuf(std::shared_ptr<TrigMesh> m);
  bool _allocated = false;
};

struct Mouse {
  bool left = false,mid=false, right = false;
  float wheel=0;
  int x = 0;
  int y = 0;
  int oldx = 0;
  int oldy = 0;
};

struct Keys {
  std::string keys;
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
};

class GLRender {
 public:
  GLRender();
  ~GLRender();
  /// only works after glfwInit() and being called in the thread
  /// that called glfwInit() because opengl limitations.
  std::string GLInfo() const;

  int Init(const std::string& vertShader, const std::string& fragShader);

  /// reallocates texture buffer
  void Resize(unsigned int width, unsigned int height);

  /// renders once
  void Render();

  void ResetCam();

  unsigned Width() const { return _width; }
  unsigned Height() const { return _height; }
  unsigned TexId() const { return _render_tex; }

  int AddMesh(std::shared_ptr<TrigMesh> mesh);

  int AllocateMeshBuffer(size_t meshId);

  int DrawMesh(size_t meshId);

  void SetMouse(int x, int y, float wheel, bool left, bool mid, bool right);

  void KeyPressed(const std::string & keys, bool ctrl, bool shift, bool alt);

  int NeedsUpdate(size_t meshId);

  int SetMeshTexture(size_t meshId, const Array2D8u& image, int numChannels);

  //overwrites existing texture image
  int SetSolidColor(size_t meshId, Vec3b color);

  int UploadMeshData(size_t meshId);

 private:
  
   unsigned _fbo = 0;
  /// renders to this texture.
  /// because renders from gpu, the cpu buffer in it is unused and empty.
  unsigned _render_tex = 0;
  unsigned _depth_buffer = 0;
  Scene scene;

  unsigned int _vertex_shader = 0, _fragment_shader = 0, _program = 0;
  /// mvp model view projection.
  /// mvit model view inverse transpose.
  unsigned int _mvp_loc = 0, _mvit_loc = 0, _light_loc = 0;
  unsigned int _tex_loc = 0;
  Camera _camera;
  Mouse _mouse;
  bool captureMouse = false;
  float camSpeed = 0.4;
  float xRotSpeed = 4e-3f, yRotSpeed = 4e-3f;
  float zoomRatio = 1.25f;

  std::string _vs_string, _fs_string;
  std::vector<GLBuf> _bufs;
  const static unsigned INIT_WIDTH = 800, INIT_HEIGHT = 800;
  unsigned _width = 0, _height = 0;
  bool _initialized = false;
};
