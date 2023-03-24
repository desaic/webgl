#include "GLRender.h"

#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "Matrix4f.h"
#include <iostream>
std::string GLRender::GLInfo() const {
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

GLRender::~GLRender() {
  //delete texture and framebuffer object
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
  glTexImage2D(GL_TEXTURE_2D, 0, format, _width,
               _height, 0, format, type, nullptr);
  glBindRenderbuffer(GL_RENDERBUFFER, _depth_buffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, _width, _height);
  _cam.ratio = width / (float)height;
}

int GLRender::DrawMesh(size_t meshId) {
  if (meshId >= _bufs.size()) {
    return -1;
  }
  const GLBuf& mesh = _bufs[meshId];
  if (mesh.b.size() == 0) {
    AllocateMeshBuffer(meshId);
  }

  return 0;
}

/// renders once
void GLRender::Render() {
  if (!_initialized) {
    return;
  }
  GLenum err;
  Matrix4f v, mvp, mvit;

  glUseProgram(_program);
  glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
  glViewport(0, 0, _width, _height);
  
  glClearColor(208 / 255.0f, 150 / 255.0f, 102 / 255.0f, 0.8);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUseProgram(0);
  for (size_t i = 0; i < _bufs.size(); i++) {
    DrawMesh(i);
  }
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
  _cam.angle_xz = 0;
  _cam.eye = Vec3f(0, 50, -200);
  _cam.at = Vec3f(0, 0, 200);
  _cam.update();
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
  while (err = glGetError()) {
    std::cout << "error " << err << "\n";
  }
  _mvp_loc = glGetUniformLocation(_program, "MVP");
  _mvit_loc = glGetUniformLocation(_program, "MVIT");
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
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _render_tex,
                       0);
  GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, DrawBuffers);
  ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (ret != GL_FRAMEBUFFER_COMPLETE) {
    std::cout << "incomplete frame buffer error "<<ret<<"\n";
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  _initialized = true;
  return 0;
}

int GLRender::AllocateMeshBuffer(size_t meshId) {
  if (meshId >= _bufs.size()) {
    return - 1;
  }
  GLBuf& buf = _bufs[meshId];
  glGenVertexArrays(1, &buf.vao);
  glBindVertexArray(buf.vao);
  buf.b.resize(GLBuf::NUM_BUF);
  glGenBuffers(GLBuf::NUM_BUF, buf.b.data());
  buf.mesh->ComputeTrigNormals();
  glBindBuffer(GL_ARRAY_BUFFER, buf.b[0]);
  const unsigned DIM = 3;
  size_t numVerts = 3 * buf.mesh->t.size();
  size_t numFloats = DIM * numVerts;
  std::vector<Vec3f> v(numVerts);
  std::vector<Vec3f> n(numVerts);
  for (size_t i = 0; i < buf.mesh->t.size(); i++) {
    Vec3f normal = *(Vec3f*)(buf.mesh->nt.data() + 3 * i);
    for (int j = 0; j < 3; j++) {
      unsigned vidx = buf.mesh->t[3 * i + j];
      v[3 * i + j] = *(Vec3f*)(buf.mesh->v.data() + 3 * vidx);
      n[3 * i + j] = normal;
    }
  }
  glBufferData(GL_ARRAY_BUFFER, numFloats * sizeof(GLfloat), v.data(),
               GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
  return 0;
}

int GLRender::AddMesh(std::shared_ptr<TrigMesh> mesh) {
  int meshId = int(_bufs.size());
  _bufs.push_back(mesh);
  return meshId;
}
