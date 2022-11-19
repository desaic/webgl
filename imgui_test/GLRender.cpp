#include "GLRender.h"
#include "GL/glew.h"
#include "GLFW/glfw3.h"

std::string GLRender::GLInfo() {
  std::string s;
  const char * strPtr = (const char*)glGetString(GL_VERSION);
  if (strPtr == nullptr) {
    return "no info";
  }
  s = std::string(strPtr);
  const char* glslVer = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
  std::string glslStr;
  if (glslVer != nullptr) {
    glslStr = std::string(glslVer);
  }
  return s + " glsl: " +  glslStr;
}

GLRender::GLRender() {

}

/// reallocates texture buffer
void GLRender::Resize(unsigned int res_x, unsigned int res_y) {
}

/// renders once
void GLRender::Render() {
}
