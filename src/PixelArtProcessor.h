#pragma once

#include <opencv2/core.hpp>

// PixelArtProcessor:
// - Independent of UI and rendering.
// - Converts a normal image into a pixel-art style image by:
//   1) Optional pre-blur (reduces noise that would pollute block colors)
//   2) Block-based representative colors (explicit N×N processing; NOT resize-based)
//   3) Palette limitation via K-means clustering OR fixed palette (color quantization)
//   4) Optional edge enhancement (makes block boundaries/edges crisper)
class PixelArtProcessor {
public:
  // Fixed palette presets for retro pixel art style
  enum class PalettePreset {
    Custom,        // Use K-means with specified paletteSize
    NES,           // Nintendo Entertainment System (54 colors)
    GameBoy,       // Original Game Boy (4 colors: dark green, light green, darker green, lightest green)
    GameBoyPocket, // Game Boy Pocket (4 colors: monochrome)
    Pico8,         // Pico-8 fantasy console (16 colors)
    CGA,           // CGA 4-color mode (4 colors)
    EGA,           // EGA 16-color mode (16 colors)
    Commodore64    // Commodore 64 (16 colors)
  };

  struct Params {
    int blockSize = 8;        // N: size of pixel blocks (4..32 typical)
    int paletteSize = 16;     // K: number of colors in final palette (only used when palettePreset == Custom)
    bool preBlur = true;      // reduces high-frequency noise before block averaging
    bool edgeEnhance = false; // optional crisp outline enhancement
    bool dither = false;      // Floyd-Steinberg dithering (reduces color banding artifacts)
    bool outline = false;     // extract contours and draw pixel-art style outlines
    int outlineThickness = 1; // outline line thickness in pixels (1-3 typical)
    PalettePreset palettePreset = PalettePreset::Custom; // Fixed palette preset or custom K-means
  };

  // Input must be 8-bit 3-channel BGR (CV_8UC3).
  // Output is 8-bit 3-channel BGR (CV_8UC3).
  static cv::Mat Process(const cv::Mat& inputBgr, const Params& params);

private:
  static cv::Mat BuildBlockColorImageBGR(const cv::Mat& inputBgr, int blockSize);
  static cv::Mat QuantizeWithKMeansLab(const cv::Mat& smallBgr, int paletteSize);
  static cv::Mat QuantizeWithFixedPalette(const cv::Mat& smallBgr, PalettePreset preset);
  static cv::Mat QuantizeWithKMeansLabDither(const cv::Mat& smallBgr, int paletteSize);
  static cv::Mat QuantizeWithFixedPaletteDither(const cv::Mat& smallBgr, PalettePreset preset);
  static cv::Mat ExpandBlocksBGR(const cv::Mat& smallBgr, const cv::Size& outSize, int blockSize);
  static void ApplyEdgeEnhancementInPlace(cv::Mat& bgr, float strength = 0.6f);
  static void ApplyPixelArtOutline(cv::Mat& bgr, int thickness = 1);
  
  // Get fixed palette colors (BGR format)
  static std::vector<cv::Vec3b> GetPaletteColors(PalettePreset preset);
  
  // Helper: find nearest palette color
  static cv::Vec3b FindNearestPaletteColor(const cv::Vec3b& color, const std::vector<cv::Vec3b>& palette);
};


