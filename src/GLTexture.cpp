#include "GLTexture.h"

#include <opencv2/imgproc.hpp>

// Include GL headers *only* in the .cpp to keep headers portable/clean.
#if defined(_WIN32)
  #include <Windows.h>
#endif
#include <GL/gl.h>

#ifndef GL_CLAMP_TO_EDGE
// Windows' legacy <GL/gl.h> (OpenGL 1.1) may not define newer constants.
// GL_CLAMP_TO_EDGE is widely supported by drivers and required to avoid edge sampling artifacts.
#define GL_CLAMP_TO_EDGE 0x812F
#endif

GLTexture::GLTexture() = default;

GLTexture::~GLTexture() {
  Destroy();
}

void GLTexture::Destroy() {
  if (textureId_ != 0) {
    GLuint id = static_cast<GLuint>(textureId_);
    glDeleteTextures(1, &id);
    textureId_ = 0;
  }
  width_ = 0;
  height_ = 0;
}

void* GLTexture::ImGuiID() const {
  // ImGui OpenGL backends treat ImTextureID as a GLuint cast to void*.
  return (void*)(intptr_t)textureId_;
}

bool GLTexture::UpdateFromMat(const cv::Mat& mat) {
  if (mat.empty()) return false;

  cv::Mat rgba;
  if (mat.type() == CV_8UC3) {
    // OpenCV default is BGR; OpenGL expects RGBA (we upload GL_RGBA).
    cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);
  } else if (mat.type() == CV_8UC4) {
    // Assume BGRA and convert to RGBA for consistent upload.
    cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
  } else if (mat.type() == CV_8UC1) {
    cv::cvtColor(mat, rgba, cv::COLOR_GRAY2RGBA);
  } else {
    return false;
  }

  if (rgba.empty() || rgba.type() != CV_8UC4) return false;

  const int w = rgba.cols;
  const int h = rgba.rows;
  if (w <= 0 || h <= 0) return false;

  if (textureId_ == 0) {
    GLuint id = 0;
    glGenTextures(1, &id);
    textureId_ = static_cast<unsigned int>(id);
  }

  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textureId_));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // GL_CLAMP_TO_EDGE avoids border artifacts when sampling near edges.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Ensure byte alignment is safe for any width (RGBA => 4 bytes per pixel, but still set explicitly).
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // If size changed, reallocate texture storage.
  if (w != width_ || h != height_) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);
    width_ = w;
    height_ = h;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}


