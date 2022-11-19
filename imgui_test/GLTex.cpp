#include "GLTex.h"
#include "GLFW/glfw3.h"

#include <iostream>

void GLTex::UpdateTextureData() {
  unsigned width = this->_width;
  unsigned format = GL_RGBA;
  unsigned type = GL_UNSIGNED_BYTE;
  if (_numChan == 1) {
    format = GL_RED;
  } else if (_numChan == 4) {
    width /= 4;
  } else {
    std::cout << "can only display gray RGB, or RGBA image.\n";
    return;
  }

  //already allocated correct GL texture buffer
  if (!_needAlloc) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, _height, format, type,
                    _buf.data());
    return;
  }

  if (_texId > 0) {
    glDeleteTextures(1, &_texId);
  }
  glGenTextures(1, &_texId);
  if (_texId == 0) {
    std::cout << "Failed to generate gl texture id\n";
    return;
  }
  GLenum errCode;
  glBindTexture(GL_TEXTURE_2D, _texId);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  // instead of opengl default 4 byte alignment
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, _height, 0, format, type,
               _buf.data());
  errCode = glGetError();
  if (errCode != GL_NO_ERROR) {
    std::cout << "error setting texture data " << errCode << "\n";
  }
  _needAlloc = false;
}
