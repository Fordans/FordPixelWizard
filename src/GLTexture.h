#pragma once

#include <opencv2/core.hpp>

// Minimal OpenGL texture wrapper for displaying OpenCV images in Dear ImGui.
// Why this exists:
// - Keeps OpenGL state/texture lifetime out of UI and processing code.
// - Central place to handle BGR->RGBA conversion, alignment, and resizing.
//
// This uses OpenGL 2.x texture calls (glTexImage2D / glTexSubImage2D),
// matching ImGui's OpenGL2 backend so we don't need an extra GL loader.
class GLTexture {
public:
  GLTexture();
  ~GLTexture();

  GLTexture(const GLTexture&) = delete;
  GLTexture& operator=(const GLTexture&) = delete;

  // Uploads an OpenCV image to the GPU as RGBA8.
  // Accepted input types:
  // - CV_8UC3 (BGR)
  // - CV_8UC4 (BGRA)
  // - CV_8UC1 (Gray)
  bool UpdateFromMat(const cv::Mat& mat);

  void Destroy();

  // For ImGui::Image: cast to ImTextureID.
  void* ImGuiID() const;

  int Width() const { return width_; }
  int Height() const { return height_; }
  bool IsValid() const { return textureId_ != 0; }

private:
  unsigned int textureId_ = 0; // GLuint, kept as unsigned int to avoid including gl headers here.
  int width_ = 0;
  int height_ = 0;
};


