#pragma once

#include <string>

#include "Scene.h"

struct ShaderBuffer {
  // vertex array object
  unsigned int vao = 0;

  // vertex and color buffer
  std::vector<unsigned int> b;
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

  Camera _cam;
  bool captureMouse = false;
  double xpos0 = 0, ypos0 = 0;
  float camSpeed = 1;
  float xRotSpeed = 4e-3f, yRotSpeed = 4e-3f;

  std::string _vs_string, _fs_string;
  std::vector<ShaderBuffer> buffers;

  const static unsigned INIT_WIDTH = 800, INIT_HEIGHT = 800;
  unsigned _width = 0, _height = 0;
  bool _initialized = false;
};
