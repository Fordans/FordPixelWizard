#ifndef APP_H
#define APP_H

#include "GLTexture.h"
#include "ImageLoader.h"
#include "PixelArtProcessor.h"

#include <array>
#include <string>

#include <imgui.h>

struct GLFWwindow;

// Main application class - encapsulates all app logic, UI, and state
class App {
public:
  App();
  ~App();

  // Initialize GLFW, window, ImGui. Returns true on success.
  bool Initialize();

  // Run the main loop until window is closed
  void Run();

  // Cleanup resources
  void Shutdown();

private:
  // Render ImGui UI (control panel + preview)
  void RenderUI();

  // Render frame (clear, render ImGui, swap buffers)
  void Render();

  // Helper: keep image display aspect ratio inside a target region
  static ImVec2 FitSizeKeepAspect(int imgW, int imgH, const ImVec2& maxSize);

  // Helper: load and set window icon from file
  static void SetWindowIcon(GLFWwindow* window, const char* iconPath);

  // Setup custom ImGui style (professional white/dark theme)
  static void SetupCustomStyle();

  // Randomize processing parameters for experimentation
  void RandomizeParams();

  // Windows file dialogs (Windows-only)
  // Returns true if user selected a file, false if cancelled
  bool ShowOpenFileDialog(char* outPath, size_t pathSize, const char* filter = nullptr);
  bool ShowSaveFileDialog(char* outPath, size_t pathSize, const char* filter = nullptr);

private:
  GLFWwindow* window_ = nullptr;

  // UI state
  std::array<char, 1024> loadPath_{};
  std::array<char, 1024> savePath_{};
  std::string status_;

  // Processing parameters
  PixelArtProcessor::Params params_;

  // Image data (BGR format for OpenCV)
  cv::Mat inputBgr_;
  cv::Mat outputBgr_;

  // OpenGL textures for display
  GLTexture inputTex_;
  GLTexture outputTex_;
};

#endif // APP_H

