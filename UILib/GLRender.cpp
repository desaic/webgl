#include "GLRender.h"

#define GLEW_STATIC
#include <iostream>

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "Matrix4f.h"
#include "Matrix3f.h"

void PrintGLError() {
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    // Handle the error
    // For example, print an error message
    std::cout<<"gl "<<error<<"\n";
  }
}

std::string GLRender::GLInfo() const {
  std::string s;
  const char* strPtr = (const char*)glGetString(GL_VERSION);
  if (strPtr == nullptr) {
    return "no info";
  }
  s = std::string(strPtr);
  const char* glslVer = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
  std::string glslStr;
  if (glslVer != nullptr) {
    glslStr = std::string(glslVer);
  }
  return s + " glsl: " + glslStr;
}

GLRender::GLRender() {}

GLRender::~GLRender() {
  // delete texture and framebuffer object
}

/// reallocates texture buffer
void GLRender::Resize(unsigned int width, unsigned int height) {
  if (_width == width && _height == height) {
    return;
  }
  _width = width;
  _height = height;
  glBindTexture(GL_TEXTURE_2D, _render_tex);
  unsigned format = GL_RGBA;
  unsigned type = GL_UNSIGNED_BYTE;
  glTexImage2D(GL_TEXTURE_2D, 0, format, _width, _height, 0, format, type,
               nullptr);
  glBindRenderbuffer(GL_RENDERBUFFER, _depth_buffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, _width, _height);
  _camera.aspect = width / (float)height;
}

int GLRender::DrawMesh(size_t meshId) {
  if (meshId >= _bufs.size()) {
    return -1;
  }
  GLBuf& buf = _bufs[meshId];
  if (!buf._allocated) {
    AllocateMeshBuffer(meshId);
  }
  if (buf._needsUpdate) {
    UploadMeshData(meshId);
  }
  if (buf._tex.HasImage()) {
    buf._tex.UpdateTextureData();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, buf._tex._texId);
    glUniform1i(_tex_loc, 0); 
  }
  glBindVertexArray(buf.vao);
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(3 * buf.mesh->GetNumTrigs()));
  return 0;
}

void GLRender::SetMouse(int x, int y, float wheel, bool left, bool mid,
                        bool right) {
  _mouse.x = x;
  _mouse.y = y;
  //rotate around _camera.at
  if (left) {
    if (_mouse.left) {
      int dx = x - _mouse.oldx;
      int dy = y - _mouse.oldy;
      _mouse.oldx = x;
      _mouse.oldy = y;
      _camera.rotate(xRotSpeed * dx, yRotSpeed * dy);
    } else {
      _mouse.oldx = x;
      _mouse.oldy = y;
    }
  }
  // pan
  else if (right) {
    if (_mouse.right) {
      int dx = x - _mouse.oldx;
      int dy = y - _mouse.oldy;
      _mouse.oldx = x;
      _mouse.oldy = y;
      _camera.pan(camSpeed * dx, camSpeed * dy);
    } else {
      _mouse.oldx = x;
      _mouse.oldy = y;
    }
  } else if (wheel != 0) {
    if (wheel > 0) {
      _camera.zoom(1.0f/zoomRatio);
    } else {
      _camera.zoom(zoomRatio);
    }
  }
  _mouse.left = left;
  _mouse.mid = mid;
  _mouse.right = right;
}

void GLRender::KeyPressed(KeyboardInput& input) {
  _keyboardInput = input;
  std::vector<std::string> keys = input.splitKeys();
  for (std::string key : keys) {
    if (key == "R" && input.ctrl) {
      ResetCam();
    }
  }
}

/// renders once
void GLRender::Render() {
  if (!_initialized) {
    return;
  }
  Matrix4f v, mvp, mvit;
  _camera.update();

  glUseProgram(_program);
  glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
  glViewport(0, 0, _width, _height);

  glClearColor(208 / 255.0f, 150 / 255.0f, 102 / 255.0f, 0.8);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  const GLsizei matCount = 1;
  const GLboolean noTranspose = GL_FALSE;
  float vp[16], vit[16];
  _camera.VP(vp);
  glUniformMatrix4fv(_mvp_loc, matCount, noTranspose, (const GLfloat*)vp);
  _camera.VIT(vit);
  glUniformMatrix4fv(_mvit_loc, matCount, noTranspose, (const GLfloat*)vit);
  for (size_t i = 0; i < _bufs.size(); i++) {
    DrawMesh(i);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUseProgram(0);
}

int checkShaderError(GLuint shader) {
  GLint success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    GLint logSize = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
    std::vector<GLchar> errorLog(logSize);
    glGetShaderInfoLog(shader, logSize, &logSize, &errorLog[0]);
    std::cout << "Shader error " << shader << " " << errorLog.data() << "\n";
    return -1;
  } else {
    std::cout << "Shader " << shader << " compiled.\n";
    return 0;
  }
}

void GLRender::ResetCam() {
  _camera.eye = Vec3f(0, 50, 100);
  _camera.at = Vec3f(0, 0, 0);
  _camera.near = 1;
  _camera.far = 500;
  _camera.update();
}

int GLRender::Init(const std::string& vertShader,
                   const std::string& fragShader) {
  _initialized = false;
  _vs_string = vertShader;
  _fs_string = fragShader;
  const char* vs_pointer = _vs_string.data();
  const char* fs_pointer = _fs_string.data();
  ResetCam();

  unsigned err = 0;
  _vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(_vertex_shader, 1, &vs_pointer, NULL);
  glCompileShader(_vertex_shader);
  int ret = checkShaderError(_vertex_shader);
  if (ret < 0) {
    return -1;
  }
  _fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(_fragment_shader, 1, &fs_pointer, NULL);
  glCompileShader(_fragment_shader);
  ret = checkShaderError(_fragment_shader);
  if (ret < 0) {
    return -2;
  }
  _program = glCreateProgram();
  glAttachShader(_program, _vertex_shader);
  glAttachShader(_program, _fragment_shader);
  glLinkProgram(_program);
  int isLinked;
  glGetProgramiv(_program, GL_LINK_STATUS, &isLinked);
  if (isLinked == GL_FALSE) {
    GLint maxLength = 0;
    glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &maxLength);

    // The maxLength includes the NULL character
    std::vector<GLchar> infoLog(maxLength);
    glGetProgramInfoLog(_program, maxLength, &maxLength, &infoLog[0]);
    std::cout << "link shader error " << infoLog.data() << "\n";
  }
  glDetachShader(_program, _vertex_shader);
  glDetachShader(_program, _fragment_shader);
  glDeleteShader(_vertex_shader);
  glDeleteShader(_fragment_shader);
  glUseProgram(_program);
  _mvp_loc = glGetUniformLocation(_program, "MVP");
  _mvit_loc = glGetUniformLocation(_program, "MVIT");
  _tex_loc = glGetUniformLocation(_program, "texSampler");
    
  glGenFramebuffers(1, &_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, _fbo);  
  glGenTextures(1, &_render_tex);
  glGenRenderbuffers(1, &_depth_buffer);
  Resize(INIT_WIDTH, INIT_HEIGHT);
  glBindTexture(GL_TEXTURE_2D, _render_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, _depth_buffer);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _render_tex, 0);
  GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, DrawBuffers);
  ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (ret != GL_FRAMEBUFFER_COMPLETE) {
    std::cout << "incomplete frame buffer error " << ret << "\n";
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  _initialized = true;
  return 0;
}

int GLRender::AllocateMeshBuffer(size_t meshId) {
  if (meshId >= _bufs.size()) {
    return -1;
  }
  GLBuf& buf = _bufs[meshId];  
  glGenVertexArrays(1, &buf.vao);
  glBindVertexArray(buf.vao);
  glGenBuffers(GLBuf::NUM_BUF, buf.b.data());
  buf._allocated = true;
  UploadMeshData(meshId);
  return 0;
}

int GLRender::UploadMeshData(size_t meshId) {
  if (meshId >= _bufs.size()) {
    return -1;
  }
  GLBuf& buf = _bufs[meshId];
  if (!buf._allocated) {
    AllocateMeshBuffer(meshId);
  }

  auto mesh = buf.mesh;
  if (mesh->nt.size() != mesh->t.size()) {
    mesh->ComputeTrigNormals();
  }
  const unsigned DIM = 3;
  buf._numTrigs = mesh->t.size() / 3;
  size_t numVerts = mesh->t.size();
  size_t numFloats = DIM * numVerts;
  size_t numUV = 2 * numVerts;
  std::vector<Vec3f> v(numVerts);
  std::vector<Vec3f> n(numVerts);
  std::vector<Vec2f> uv;
  if (mesh->uv.size() != numVerts) {
    // generate uv by placing each triangle on its own pixel to have
    // per-triangle coloring
    unsigned TexWidth = std::sqrt(mesh->GetNumTrigs() + 1) + 1;
    float pixelSize = 1.0f / TexWidth;
    uv.resize(numVerts, Vec2f(0, 0));
    for (unsigned i = 0; i < numVerts; i++) {
      unsigned t = i / 3;
      unsigned row = i / TexWidth;
      unsigned col = i % TexWidth;
      uv[i] = Vec2f((col + 0.5f) * pixelSize, (row + 0.5f) * pixelSize);
    }
  } else {
    uv = mesh->uv;
  }
  for (size_t i = 0; i < mesh->t.size(); i += 3) {
    Vec3f normal = *(Vec3f*)(mesh->nt.data() + i);
    for (int j = 0; j < 3; j++) {
      unsigned vidx = mesh->t[i + j];
      unsigned dstV = i + j;
      v[dstV] = *(Vec3f*)(mesh->v.data() + 3 * vidx);
      n[dstV] = normal;
    }
  }
  glBindVertexArray(buf.vao);
  glBindBuffer(GL_ARRAY_BUFFER, buf.b[0]);
  glBufferData(GL_ARRAY_BUFFER, numFloats * sizeof(GLfloat), v.data(),
               GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

  glBindBuffer(GL_ARRAY_BUFFER, buf.b[1]);
  glBufferData(GL_ARRAY_BUFFER, numFloats * sizeof(GLfloat), n.data(),
               GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

  glBindBuffer(GL_ARRAY_BUFFER, buf.b[2]);
  glBufferData(GL_ARRAY_BUFFER, numUV * sizeof(GLfloat), uv.data(),
               GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
  buf._needsUpdate = false;
  return 0;
}

int GLRender::AddMesh(std::shared_ptr<TrigMesh> mesh) {
  int meshId = int(_bufs.size());
  _bufs.push_back(mesh);
  return meshId;
}

int GLRender::SetNeedsUpdate(size_t meshId) {
  if (meshId >= _bufs.size()) {
    return -1;
  }
  _bufs[meshId]._needsUpdate = true;
  return 0;
}

int GLRender::SetMeshTexture(size_t meshId, const Array2D8u& image,
                             int numChannels) {
  if (meshId >= _bufs.size()) {
    return -1;
  }
  Vec2u size = image.GetSize();
  _bufs[meshId]._tex.SetImageData(image.DataPtr(), size[0], size[1],
                                 numChannels);
  return 0;
}

int GLRender::SetSolidColor(size_t meshId, Vec3b color) {
  if (meshId >= _bufs.size()) {
    return -1;
  }
  auto& buf = _bufs[meshId]._tex._buf;
  for (unsigned i = 0; i < buf.size(); i += 4) {
    buf[i] = color[0];
    buf[i + 1] = color[1];
    buf[i + 2] = color[2];
    buf[i + 2] = 255;
  }
  _bufs[meshId]._tex._needUpdate = true;
  return 0;
}

GLBuf::GLBuf(std::shared_ptr<TrigMesh> m) : mesh(m), b(NUM_BUF) {
  _tex.TexWrapParam = GL_REPEAT;
  _tex.MakeDefaultTex();
}