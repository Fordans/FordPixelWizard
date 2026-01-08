#include "App.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>

// Include OpenCV BEFORE Windows.h to avoid min/max macro conflicts
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <random>

#include <random>

#include <GLFW/glfw3.h>
#include <gl/GL.h>

// Windows-specific includes (after OpenCV)
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX  // Prevent Windows.h from defining min/max macros
#include <windows.h>
#include <commdlg.h>
#endif

App::App() {
  // Initialize default parameters
  params_.blockSize = 8;
  params_.paletteSize = 16;
  params_.preBlur = true;
  params_.edgeEnhance = false;
}

App::~App() {
  Shutdown();
}

bool App::Initialize() {
  // --- GLFW init ---
  if (!glfwInit()) {
    return false;
  }

  // OpenGL 2.0 context is enough for ImGui OpenGL2 backend
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  window_ = glfwCreateWindow(1280, 720, "FordPixelWizard", nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1); // vsync

  // Set window icon (for taskbar/Alt+Tab display)
  SetWindowIcon(window_, "icon.png");

  // --- ImGui init ---
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  
  // Scale up UI for better visibility
  io.FontGlobalScale = 1.2f;  // 20% larger text
  
  // Apply custom professional style instead of default
  SetupCustomStyle();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL2_Init();

  return true;
}

void App::Run() {
  // Main loop
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    RenderUI();

    Render();
  }
}

void App::Shutdown() {
  // Cleanup textures
  outputTex_.Destroy();
  inputTex_.Destroy();

  // Cleanup ImGui
  if (window_) {
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  // Cleanup GLFW
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

void App::RenderUI() {
  // Get viewport to calculate window sizes
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 viewportSize = viewport->Size;
  
  // Calculate split: preview panel on left (~60%), controls panel on right (~40%)
  float previewWidth = viewportSize.x * 0.6f;
  float controlsWidth = viewportSize.x - previewWidth;
  float panelHeight = viewportSize.y;

  // Window flags: no title bar, no move, no resize, no docking - fixed to viewport
  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar 
                                | ImGuiWindowFlags_NoMove 
                                | ImGuiWindowFlags_NoResize 
                                | ImGuiWindowFlags_NoCollapse
                                | ImGuiWindowFlags_NoBringToFrontOnFocus
                                | ImGuiWindowFlags_NoNavFocus;

  // Left panel: Preview (with original and result side-by-side)
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(previewWidth, panelHeight));
  ImGui::Begin("Preview", nullptr, windowFlags);
  
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float halfW = (avail.x - 10.0f) * 0.5f;
  float h = avail.y;
  if (halfW < 50.0f) halfW = avail.x; // fallback if narrow

  // Left: original
  ImGui::BeginChild("orig", ImVec2(halfW, h), true);
  ImGui::TextUnformatted("Original");
  if (inputTex_.IsValid()) {
    ImVec2 maxSize = ImGui::GetContentRegionAvail();
    ImVec2 sz = FitSizeKeepAspect(inputTex_.Width(), inputTex_.Height(), maxSize);
    ImGui::Image(inputTex_.ImGuiID(), sz);
  } else {
    ImGui::TextUnformatted("No image loaded.");
  }
  ImGui::EndChild();

  ImGui::SameLine();

  // Right: result
  ImGui::BeginChild("result", ImVec2(0, h), true);
  ImGui::TextUnformatted("Pixel Art Result");
  if (outputTex_.IsValid()) {
    ImVec2 maxSize = ImGui::GetContentRegionAvail();
    ImVec2 sz = FitSizeKeepAspect(outputTex_.Width(), outputTex_.Height(), maxSize);
    ImGui::Image(outputTex_.ImGuiID(), sz);
  } else {
    ImGui::TextUnformatted("No result yet. Click Pixelize.");
  }
  ImGui::EndChild();

  ImGui::End();

  // Right panel: Controls (parameters and actions)
  ImGui::SetNextWindowPos(ImVec2(previewWidth, 0));
  ImGui::SetNextWindowSize(ImVec2(controlsWidth, panelHeight));
  ImGui::Begin("FordPixelWizard", nullptr, windowFlags);
  
  ImGui::TextUnformatted("Load / Process / Save");

  ImGui::Separator();
  ImGui::TextUnformatted("Load Image (png/jpg):");
  ImGui::InputText("##load_path", loadPath_.data(), loadPath_.size());
  ImGui::SameLine();
  if (ImGui::Button("Browse...")) {
    char selectedPath[1024] = {};
    if (ShowOpenFileDialog(selectedPath, sizeof(selectedPath))) {
      strncpy_s(loadPath_.data(), loadPath_.size(), selectedPath, _TRUNCATE);
      // Auto-load after selection
      std::string err;
      cv::Mat img;
      if (ImageLoader::LoadBGR(loadPath_.data(), img, err)) {
        inputBgr_ = img;
        outputBgr_.release();
        inputTex_.UpdateFromMat(inputBgr_);
        outputTex_.Destroy();
        status_ = "Loaded: " + std::string(loadPath_.data());
      } else {
        status_ = "Load failed: " + err;
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load")) {
    std::string err;
    cv::Mat img;
    if (ImageLoader::LoadBGR(loadPath_.data(), img, err)) {
      inputBgr_ = img;
      outputBgr_.release();
      inputTex_.UpdateFromMat(inputBgr_);
      outputTex_.Destroy();
      status_ = "Loaded: " + std::string(loadPath_.data());
    } else {
      status_ = "Load failed: " + err;
    }
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Pixel Art Params:");
  ImGui::SliderInt("Block Size", &params_.blockSize, 4, 32);
  
  // Palette selection
  ImGui::TextUnformatted("Palette:");
  const char* paletteNames[] = {
    "Custom (K-means)",
    "NES (54 colors)",
    "Game Boy (4 colors)",
    "Game Boy Pocket (4 colors)",
    "Pico-8 (16 colors)",
    "CGA (4 colors)",
    "EGA (16 colors)",
    "Commodore 64 (16 colors)"
  };
  int currentPalette = static_cast<int>(params_.palettePreset);
  if (ImGui::Combo("##palette", &currentPalette, paletteNames, IM_ARRAYSIZE(paletteNames))) {
    params_.palettePreset = static_cast<PixelArtProcessor::PalettePreset>(currentPalette);
  }
  
  // Show palette size slider only for Custom palette
  if (params_.palettePreset == PixelArtProcessor::PalettePreset::Custom) {
    ImGui::SliderInt("Palette Size", &params_.paletteSize, 2, 64);
  } else {
    // Display palette info for fixed palettes
    int paletteColorCount = 0;
    switch (params_.palettePreset) {
      case PixelArtProcessor::PalettePreset::NES: paletteColorCount = 54; break;
      case PixelArtProcessor::PalettePreset::GameBoy:
      case PixelArtProcessor::PalettePreset::GameBoyPocket:
      case PixelArtProcessor::PalettePreset::CGA: paletteColorCount = 4; break;
      case PixelArtProcessor::PalettePreset::Pico8:
      case PixelArtProcessor::PalettePreset::EGA:
      case PixelArtProcessor::PalettePreset::Commodore64: paletteColorCount = 16; break;
      default: break;
    }
    ImGui::TextDisabled("Using fixed %d-color palette", paletteColorCount);
  }
  
  ImGui::Checkbox("Pre-Blur (reduce noise)", &params_.preBlur);
  ImGui::Checkbox("Edge Enhance (crisper outlines)", &params_.edgeEnhance);
  ImGui::Checkbox("Floyd-Steinberg Dither (reduce color banding)", &params_.dither);
  
  ImGui::Checkbox("Outline (contour extraction + pixel-art borders)", &params_.outline);
  if (params_.outline) {
    ImGui::SliderInt("Outline Thickness", &params_.outlineThickness, 1, 3);
  }

  ImGui::Separator();
  if (ImGui::Button("Random Config")) {
    RandomizeParams();
    status_ = "Parameters randomized! Click 'Pixelize' to apply.";
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted("Randomize all parameters to discover new pixel art styles!");
    ImGui::EndTooltip();
  }

  if (ImGui::Button("Pixelize (Pixel Art)")) {
    if (inputBgr_.empty()) {
      status_ = "No input image loaded.";
    } else {
      outputBgr_ = PixelArtProcessor::Process(inputBgr_, params_);
      if (outputBgr_.empty()) {
        status_ = "Processing failed (unexpected empty output).";
      } else {
        outputTex_.UpdateFromMat(outputBgr_);
        status_ = "Processed successfully. Preview updated.";
      }
    }
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Save Result:");
  if (ImGui::Button("Save")) {
    if (outputBgr_.empty()) {
      status_ = "Nothing to save (process an image first).";
    } else {
      // If no save path is set, open save dialog first
      if (savePath_[0] == '\0') {
        char selectedPath[1024] = {};
        if (ShowSaveFileDialog(selectedPath, sizeof(selectedPath))) {
          strncpy_s(savePath_.data(), savePath_.size(), selectedPath, _TRUNCATE);
        } else {
          status_ = "Save cancelled.";
          ImGui::End();
          return; // User cancelled, don't save
        }
      }
      
      // Save the image
      std::string err;
      if (ImageLoader::Save(savePath_.data(), outputBgr_, err)) {
        status_ = "Saved: " + std::string(savePath_.data());
      } else {
        status_ = "Save failed: " + err;
      }
    }
  }

  ImGui::Separator();
  if (!status_.empty()) {
    ImGui::TextWrapped("%s", status_.c_str());
  }
  ImGui::End();
}

void App::RandomizeParams() {
  // Use C++11 random number generator
  static std::random_device rd;
  static std::mt19937 gen(rd());
  
  // Randomize block size (4-32, prefer values that divide nicely)
  std::uniform_int_distribution<int> blockDist(4, 32);
  params_.blockSize = blockDist(gen);
  
  // Randomize palette preset (all available presets)
  std::uniform_int_distribution<int> palettePresetDist(
      0, static_cast<int>(PixelArtProcessor::PalettePreset::Commodore64));
  params_.palettePreset = static_cast<PixelArtProcessor::PalettePreset>(
      palettePresetDist(gen));
  
  // Randomize palette size (2-64, only used when Custom)
  std::uniform_int_distribution<int> paletteSizeDist(2, 64);
  params_.paletteSize = paletteSizeDist(gen);
  
  // Randomize boolean options (each has 50% chance)
  std::uniform_int_distribution<int> boolDist(0, 1);
  params_.preBlur = (boolDist(gen) == 1);
  params_.edgeEnhance = (boolDist(gen) == 1);
  params_.dither = (boolDist(gen) == 1);
  params_.outline = (boolDist(gen) == 1);
  
  // Randomize outline thickness (1-3, only if outline is enabled)
  if (params_.outline) {
    std::uniform_int_distribution<int> thicknessDist(1, 3);
    params_.outlineThickness = thicknessDist(gen);
  } else {
    params_.outlineThickness = 1; // Reset to default if outline disabled
  }
}

void App::Render() {
  // Render ImGui
  ImGui::Render();
  int display_w = 0, display_h = 0;
  glfwGetFramebufferSize(window_, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  // Match background color with ImGui dark theme
  glClearColor(0.12f, 0.12f, 0.14f, 1.00f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

  glfwSwapBuffers(window_);
}

ImVec2 App::FitSizeKeepAspect(int imgW, int imgH, const ImVec2& maxSize) {
  if (imgW <= 0 || imgH <= 0) return ImVec2(0, 0);
  const float aspect = static_cast<float>(imgW) / static_cast<float>(imgH);
  float w = maxSize.x;
  float h = maxSize.y;
  if (w <= 0.0f || h <= 0.0f) return ImVec2(0, 0);

  float outW = w;
  float outH = outW / aspect;
  if (outH > h) {
    outH = h;
    outW = outH * aspect;
  }
  return ImVec2(outW, outH);
}

void App::SetWindowIcon(GLFWwindow* window, const char* iconPath) {
  if (!window || !iconPath) return;

  // Use IMREAD_UNCHANGED to preserve alpha channel (transparency)
  cv::Mat iconSrc = cv::imread(iconPath, cv::IMREAD_UNCHANGED);
  if (iconSrc.empty()) {
    // Icon file not found or failed to load - not critical, just skip
    return;
  }

  cv::Mat iconRgba;

  // Handle different input formats and convert to RGBA
  if (iconSrc.channels() == 4) {
    // Already BGRA, convert to RGBA (swap B and R, keep A)
    cv::cvtColor(iconSrc, iconRgba, cv::COLOR_BGRA2RGBA);
  } else if (iconSrc.channels() == 3) {
    // BGR input, add opaque alpha channel and convert to RGBA
    cv::Mat iconBgr = iconSrc;
    cv::cvtColor(iconBgr, iconRgba, cv::COLOR_BGR2RGBA);
  } else if (iconSrc.channels() == 1) {
    // Grayscale, convert to RGBA (grayscale with opaque alpha)
    cv::cvtColor(iconSrc, iconRgba, cv::COLOR_GRAY2RGBA);
  } else {
    // Unsupported format
    return;
  }

  // GLFWimage expects pixel data in RGBA format, row-major
  // GLFW will copy the pixel data internally, so we can safely use iconRgba.data
  // (which remains valid until iconRgba goes out of scope)
  GLFWimage glfwImg;
  glfwImg.width = iconRgba.cols;
  glfwImg.height = iconRgba.rows;
  glfwImg.pixels = iconRgba.data;

  // Set window icon (GLFW copies the pixel data internally, so iconRgba can be destroyed after this call)
  glfwSetWindowIcon(window, 1, &glfwImg);
}

void App::SetupCustomStyle() {
  ImGuiStyle& style = ImGui::GetStyle();
  
  // Reset to base style
  style = ImGuiStyle();
  
  // Professional color scheme: white and dark tones
  // Base colors
  ImVec4 darkBg = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);      // Dark background
  ImVec4 panelBg = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);     // Panel background
  ImVec4 border = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);      // Border color
  ImVec4 text = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);        // Text (nearly white)
  ImVec4 textDim = ImVec4(0.70f, 0.70f, 0.75f, 1.00f);     // Dimmed text
  
  // Interactive elements
  ImVec4 button = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);      // Button base
  ImVec4 buttonHover = ImVec4(0.35f, 0.35f, 0.40f, 1.00f); // Button hover
  ImVec4 buttonActive = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);// Button active
  ImVec4 sliderGrab = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);  // Slider grab (white)
  ImVec4 sliderBg = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);    // Slider background
  
  // Accent/Highlight colors (subtle white/light gray accents)
  ImVec4 header = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);      // Header background
  ImVec4 headerHover = ImVec4(0.28f, 0.28f, 0.32f, 1.00f); // Header hover
  ImVec4 checkbox = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);    // Checkbox
  
  // Apply colors
  style.Colors[ImGuiCol_Text] = text;
  style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
  style.Colors[ImGuiCol_WindowBg] = darkBg;
  style.Colors[ImGuiCol_ChildBg] = panelBg;
  style.Colors[ImGuiCol_PopupBg] = panelBg;
  style.Colors[ImGuiCol_Border] = border;
  style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
  style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
  style.Colors[ImGuiCol_TitleBg] = header;
  style.Colors[ImGuiCol_TitleBgActive] = header;
  style.Colors[ImGuiCol_TitleBgCollapsed] = header;
  style.Colors[ImGuiCol_MenuBarBg] = panelBg;
  style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
  style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
  style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
  style.Colors[ImGuiCol_CheckMark] = sliderGrab;
  style.Colors[ImGuiCol_SliderGrab] = sliderGrab;
  style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  style.Colors[ImGuiCol_Button] = button;
  style.Colors[ImGuiCol_ButtonHovered] = buttonHover;
  style.Colors[ImGuiCol_ButtonActive] = buttonActive;
  style.Colors[ImGuiCol_Header] = header;
  style.Colors[ImGuiCol_HeaderHovered] = headerHover;
  style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
  style.Colors[ImGuiCol_Separator] = border;
  style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
  style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
  style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.30f, 0.35f, 0.50f);
  style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.45f, 0.45f, 0.50f, 0.75f);
  style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
  style.Colors[ImGuiCol_Tab] = header;
  style.Colors[ImGuiCol_TabHovered] = headerHover;
  style.Colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
  style.Colors[ImGuiCol_PlotLines] = textDim;
  style.Colors[ImGuiCol_PlotLinesHovered] = text;
  style.Colors[ImGuiCol_PlotHistogram] = textDim;
  style.Colors[ImGuiCol_PlotHistogramHovered] = text;
  style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.40f, 0.40f, 0.50f, 0.50f);
  style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
  style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
  style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
  
  // Style settings for professional look - enlarged for better visibility
  style.WindowPadding = ImVec2(16.0f, 16.0f);      // Increased from 10,10
  style.FramePadding = ImVec2(12.0f, 8.0f);        // Increased from 8,4
  style.ItemSpacing = ImVec2(12.0f, 10.0f);        // Increased from 8,6
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);     // Increased from 6,4
  style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
  style.IndentSpacing = 28.0f;                     // Increased from 21
  style.ScrollbarSize = 18.0f;                     // Increased from 14
  style.GrabMinSize = 16.0f;                       // Increased from 10
  
  // Window and frame rounding (subtle rounding for modern look)
  style.WindowBorderSize = 0.0f;  // No window border (cleaner)
  style.WindowRounding = 0.0f;    // Sharp corners (professional)
  style.ChildBorderSize = 1.0f;
  style.ChildRounding = 6.0f;     // Increased from 4.0f
  style.FrameBorderSize = 0.0f;   // No frame border
  style.FrameRounding = 3.0f;     // Increased from 2.0f for larger buttons/inputs
  style.PopupBorderSize = 1.0f;
  style.PopupRounding = 6.0f;     // Increased from 4.0f
  style.GrabRounding = 3.0f;      // Increased from 2.0f
  style.TabRounding = 3.0f;       // Increased from 2.0f
  style.ScrollbarRounding = 6.0f; // Increased from 4.0f
  
  // Layout and alignment
  style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
  style.WindowMenuButtonPosition = ImGuiDir_None;  // Hide menu button (cleaner)
  style.ColorButtonPosition = ImGuiDir_Right;
  style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
  style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
  style.DisplaySafeAreaPadding = ImVec2(3.0f, 3.0f);
  
  // Disable default ImGui "face" look
  style.Alpha = 1.0f;
  style.DisabledAlpha = 0.6f;
  style.AntiAliasedLines = true;
  style.AntiAliasedLinesUseTex = true;
  style.AntiAliasedFill = true;
  style.CurveTessellationTol = 1.25f;
  style.CircleTessellationMaxError = 0.30f;
}

#ifdef _WIN32
bool App::ShowOpenFileDialog(char* outPath, size_t pathSize, const char* filter) {
  if (!outPath || pathSize == 0) return false;

  OPENFILENAMEA ofn = {};
  char szFile[1024] = {};
  
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = glfwGetWin32Window(window_);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = filter ? filter : "Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0PNG Files\0*.png\0JPEG Files\0*.jpg;*.jpeg\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = nullptr;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = nullptr;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameA(&ofn)) {
    strncpy_s(outPath, pathSize, szFile, _TRUNCATE);
    return true;
  }
  return false;
}

bool App::ShowSaveFileDialog(char* outPath, size_t pathSize, const char* filter) {
  if (!outPath || pathSize == 0) return false;

  OPENFILENAMEA ofn = {};
  char szFile[1024] = {};
  
  // Pre-fill with default filename if savePath_ has something
  if (savePath_[0] != '\0') {
    strncpy_s(szFile, sizeof(szFile), savePath_.data(), _TRUNCATE);
  }
  
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = glfwGetWin32Window(window_);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = filter ? filter : "PNG Files\0*.png\0JPEG Files\0*.jpg\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = "png";
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

  if (GetSaveFileNameA(&ofn)) {
    strncpy_s(outPath, pathSize, szFile, _TRUNCATE);
    return true;
  }
  return false;
}
#else
bool App::ShowOpenFileDialog(char* outPath, size_t pathSize, const char* filter) {
  (void)outPath;
  (void)pathSize;
  (void)filter;
  return false; // Not implemented on non-Windows platforms
}

bool App::ShowSaveFileDialog(char* outPath, size_t pathSize, const char* filter) {
  (void)outPath;
  (void)pathSize;
  (void)filter;
  return false; // Not implemented on non-Windows platforms
}
#endif

