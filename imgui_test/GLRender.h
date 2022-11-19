#pragma once

#include <string>

#include "GLTex.h"
#include "Scene.h"

class GLRender {

 public:

  GLRender();
   
  /// only works after glfwInit() and being called in the thread
  /// that called glfwInit() because opengl limitations.
  std::string GLInfo();

  /// reallocates texture buffer
  void Resize(unsigned int res_x, unsigned int res_y);

  /// renders once
  void Render();

 private:

  /// renders to this texture. 
  /// because renders from gpu, the cpu buffer in it is unused and empty.
  GLTex _renderTex;

  Scene scene;
};
