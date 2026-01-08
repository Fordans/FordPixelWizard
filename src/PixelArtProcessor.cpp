#include "PixelArtProcessor.h"

#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>
#include <opencv2/imgproc.hpp>

namespace {
inline int ClampInt(int v, int lo, int hi) { return std::max(lo, std::min(v, hi)); }

// Helper: compute squared Euclidean distance in RGB space
inline float ColorDistanceSquared(const cv::Vec3b& a, const cv::Vec3b& b) {
  const float dr = static_cast<float>(a[2]) - static_cast<float>(b[2]);
  const float dg = static_cast<float>(a[1]) - static_cast<float>(b[1]);
  const float db = static_cast<float>(a[0]) - static_cast<float>(b[0]);
  return dr * dr + dg * dg + db * db;
}
} // namespace

cv::Mat PixelArtProcessor::Process(const cv::Mat& inputBgr, const Params& params) {
  if (inputBgr.empty()) return {};
  if (inputBgr.type() != CV_8UC3) return {};

  Params p = params;
  p.blockSize = ClampInt(p.blockSize, 1, 256);
  p.paletteSize = ClampInt(p.paletteSize, 2, 256);

  cv::Mat work = inputBgr.clone();

  // Step 1: Optional pre-blur.
  // Why: pixel-art block averaging is sensitive to salt-and-pepper noise and fine texture.
  // A small Gaussian blur nudges the block representative colors toward stable "flat" colors.
  if (p.preBlur) {
    // Kernel size must be odd. Keep it modest relative to block size.
    int k = std::max(3, (p.blockSize / 2) | 1);
    cv::GaussianBlur(work, work, cv::Size(k, k), 0.0, 0.0, cv::BORDER_DEFAULT);
  }

  // Step 2: Explicit block-based representative color image.
  // IMPORTANT: This is not resize-based downsampling; we iterate blocks and compute per-block mean.
  cv::Mat smallBlocksBgr = BuildBlockColorImageBGR(work, p.blockSize);
  if (smallBlocksBgr.empty()) return {};

  // Step 3: Palette limitation
  // - If Custom: use K-means clustering in Lab space (perceptual color quantization)
  // - If fixed preset: quantize to predefined retro palette (NES/GB/Pico-8/etc)
  // - Optionally apply Floyd-Steinberg dithering to reduce color banding
  cv::Mat quantizedSmallBgr;
  if (p.dither) {
    // Use dithering versions
    if (p.palettePreset == PalettePreset::Custom) {
      quantizedSmallBgr = QuantizeWithKMeansLabDither(smallBlocksBgr, p.paletteSize);
    } else {
      quantizedSmallBgr = QuantizeWithFixedPaletteDither(smallBlocksBgr, p.palettePreset);
    }
  } else {
    // Standard quantization without dithering
    if (p.palettePreset == PalettePreset::Custom) {
      quantizedSmallBgr = QuantizeWithKMeansLab(smallBlocksBgr, p.paletteSize);
    } else {
      quantizedSmallBgr = QuantizeWithFixedPalette(smallBlocksBgr, p.palettePreset);
    }
  }
  if (quantizedSmallBgr.empty()) return {};

  // Step 4: Expand blocks back to full resolution by filling each N×N block with its quantized color.
  cv::Mat out = ExpandBlocksBGR(quantizedSmallBgr, inputBgr.size(), p.blockSize);

  // Step 5 (optional): Edge enhancement on the final pixelated result.
  // Why: pixel art often has crisp separations; a gentle unsharp mask helps emphasize edges
  // without reintroducing continuous-tone gradients.
  if (p.edgeEnhance && !out.empty()) {
    ApplyEdgeEnhancementInPlace(out, 0.7f);
  }

  // Step 6 (optional): Extract contours and draw pixel-art style outlines.
  // Why: outlines give pixel art a distinctive cartoon-like appearance, separating objects from background.
  if (p.outline && !out.empty()) {
    p.outlineThickness = ClampInt(p.outlineThickness, 1, 5);
    ApplyPixelArtOutline(out, p.outlineThickness);
  }
  return out;
}

cv::Mat PixelArtProcessor::BuildBlockColorImageBGR(const cv::Mat& inputBgr, int blockSize) {
  const int w = inputBgr.cols;
  const int h = inputBgr.rows;
  if (w <= 0 || h <= 0) return {};
  blockSize = std::max(1, blockSize);

  const int bw = (w + blockSize - 1) / blockSize;
  const int bh = (h + blockSize - 1) / blockSize;

  cv::Mat small(bh, bw, CV_8UC3, cv::Scalar(0, 0, 0));

  for (int by = 0; by < bh; ++by) {
    for (int bx = 0; bx < bw; ++bx) {
      const int x0 = bx * blockSize;
      const int y0 = by * blockSize;
      const int x1 = std::min(x0 + blockSize, w);
      const int y1 = std::min(y0 + blockSize, h);

      cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
      cv::Mat block = inputBgr(roi);

      // Representative color: mean color in BGR.
      // Why mean: fast, stable, and works well once we apply a mild pre-blur.
      const cv::Scalar m = cv::mean(block);
      small.at<cv::Vec3b>(by, bx) = cv::Vec3b(
          static_cast<uchar>(m[0]),
          static_cast<uchar>(m[1]),
          static_cast<uchar>(m[2]));
    }
  }
  return small;
}

cv::Mat PixelArtProcessor::QuantizeWithKMeansLab(const cv::Mat& smallBgr, int paletteSize) {
  if (smallBgr.empty() || smallBgr.type() != CV_8UC3) return {};

  const int total = smallBgr.rows * smallBgr.cols;
  if (total <= 0) return {};

  paletteSize = std::max(2, std::min(paletteSize, total));

  // Convert to Lab for perceptual clustering.
  cv::Mat smallLab;
  cv::cvtColor(smallBgr, smallLab, cv::COLOR_BGR2Lab);

  // Flatten to N x 3 float samples for kmeans.
  cv::Mat samples(total, 3, CV_32F);
  for (int y = 0; y < smallLab.rows; ++y) {
    const cv::Vec3b* row = smallLab.ptr<cv::Vec3b>(y);
    for (int x = 0; x < smallLab.cols; ++x) {
      const int i = y * smallLab.cols + x;
      samples.at<float>(i, 0) = static_cast<float>(row[x][0]);
      samples.at<float>(i, 1) = static_cast<float>(row[x][1]);
      samples.at<float>(i, 2) = static_cast<float>(row[x][2]);
    }
  }

  cv::Mat labels;
  cv::Mat centers;
  const int attempts = 3;
  cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1.0);

  // K-means clustering in Lab.
  // NOTE: We keep data small by clustering only the per-block representative colors.
  // This is much faster than clustering every original pixel while still producing a clean palette.
  cv::kmeans(samples, paletteSize, labels, criteria, attempts, cv::KMEANS_PP_CENTERS, centers);

  // Reconstruct quantized small image in Lab using cluster centers, then convert back to BGR.
  cv::Mat quantLab(smallLab.size(), CV_8UC3);
  for (int y = 0; y < quantLab.rows; ++y) {
    cv::Vec3b* outRow = quantLab.ptr<cv::Vec3b>(y);
    for (int x = 0; x < quantLab.cols; ++x) {
      const int i = y * quantLab.cols + x;
      const int label = labels.at<int>(i, 0);
      const cv::Vec3f c(
          centers.at<float>(label, 0),
          centers.at<float>(label, 1),
          centers.at<float>(label, 2));

      outRow[x] = cv::Vec3b(
          static_cast<uchar>(ClampInt(static_cast<int>(std::lround(c[0])), 0, 255)),
          static_cast<uchar>(ClampInt(static_cast<int>(std::lround(c[1])), 0, 255)),
          static_cast<uchar>(ClampInt(static_cast<int>(std::lround(c[2])), 0, 255)));
    }
  }

  cv::Mat quantBgr;
  cv::cvtColor(quantLab, quantBgr, cv::COLOR_Lab2BGR);
  return quantBgr;
}

cv::Mat PixelArtProcessor::ExpandBlocksBGR(const cv::Mat& smallBgr, const cv::Size& outSize, int blockSize) {
  if (smallBgr.empty() || smallBgr.type() != CV_8UC3) return {};
  blockSize = std::max(1, blockSize);

  cv::Mat out(outSize, CV_8UC3, cv::Scalar(0, 0, 0));

  const int bw = smallBgr.cols;
  const int bh = smallBgr.rows;

  for (int by = 0; by < bh; ++by) {
    for (int bx = 0; bx < bw; ++bx) {
      const cv::Vec3b c = smallBgr.at<cv::Vec3b>(by, bx);

      const int x0 = bx * blockSize;
      const int y0 = by * blockSize;
      const int x1 = std::min(x0 + blockSize, out.cols);
      const int y1 = std::min(y0 + blockSize, out.rows);
      if (x0 >= x1 || y0 >= y1) continue;

      cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
      out(roi).setTo(cv::Scalar(c[0], c[1], c[2]));
    }
  }

  return out;
}

void PixelArtProcessor::ApplyEdgeEnhancementInPlace(cv::Mat& bgr, float strength) {
  if (bgr.empty() || bgr.type() != CV_8UC3) return;
  strength = std::max(0.0f, std::min(strength, 2.0f));

  // Simple unsharp mask:
  // - Blur a bit
  // - Add back high-frequency component
  cv::Mat blurred;
  cv::GaussianBlur(bgr, blurred, cv::Size(3, 3), 0.0, 0.0, cv::BORDER_DEFAULT);

  cv::Mat bgrF, blurredF;
  bgr.convertTo(bgrF, CV_32F);
  blurred.convertTo(blurredF, CV_32F);

  cv::Mat high = bgrF - blurredF;
  cv::Mat sharpened = bgrF + high * strength;

  // Clamp and convert back.
  sharpened = cv::min(cv::max(sharpened, 0.0f), 255.0f);
  sharpened.convertTo(bgr, CV_8UC3);
}

std::vector<cv::Vec3b> PixelArtProcessor::GetPaletteColors(PalettePreset preset) {
  std::vector<cv::Vec3b> colors;
  
  switch (preset) {
    case PalettePreset::NES: {
      // NES palette (54 colors) - classic console colors
      // Reference: https://lospec.com/palette-list/nes-palette
      colors = {
        cv::Vec3b(124, 124, 124), cv::Vec3b(0, 0, 252), cv::Vec3b(0, 0, 188), cv::Vec3b(68, 40, 188),
        cv::Vec3b(148, 0, 132), cv::Vec3b(168, 0, 32), cv::Vec3b(168, 16, 0), cv::Vec3b(136, 20, 0),
        cv::Vec3b(80, 48, 0), cv::Vec3b(0, 120, 0), cv::Vec3b(0, 104, 0), cv::Vec3b(0, 88, 0),
        cv::Vec3b(0, 64, 88), cv::Vec3b(0, 0, 0), cv::Vec3b(0, 0, 0), cv::Vec3b(188, 188, 188),
        cv::Vec3b(0, 120, 248), cv::Vec3b(0, 88, 248), cv::Vec3b(104, 68, 252), cv::Vec3b(216, 0, 204),
        cv::Vec3b(228, 0, 88), cv::Vec3b(248, 56, 0), cv::Vec3b(228, 92, 16), cv::Vec3b(172, 124, 0),
        cv::Vec3b(0, 184, 0), cv::Vec3b(0, 168, 0), cv::Vec3b(0, 168, 68), cv::Vec3b(0, 136, 136),
        cv::Vec3b(248, 248, 248), cv::Vec3b(60, 188, 252), cv::Vec3b(104, 136, 252), cv::Vec3b(152, 120, 248),
        cv::Vec3b(248, 120, 248), cv::Vec3b(248, 88, 152), cv::Vec3b(248, 120, 88), cv::Vec3b(252, 160, 68),
        cv::Vec3b(248, 184, 0), cv::Vec3b(184, 248, 24), cv::Vec3b(88, 216, 84), cv::Vec3b(88, 248, 152),
        cv::Vec3b(0, 232, 216), cv::Vec3b(120, 120, 120), cv::Vec3b(252, 252, 252), cv::Vec3b(164, 228, 252),
        cv::Vec3b(184, 184, 248), cv::Vec3b(216, 184, 248), cv::Vec3b(248, 184, 248), cv::Vec3b(248, 164, 192),
        cv::Vec3b(240, 208, 176), cv::Vec3b(252, 224, 168), cv::Vec3b(248, 216, 120), cv::Vec3b(216, 248, 120),
        cv::Vec3b(184, 248, 184), cv::Vec3b(184, 248, 216), cv::Vec3b(0, 252, 252), cv::Vec3b(248, 216, 248)
      };
      break;
    }
    
    case PalettePreset::GameBoy: {
      // Original Game Boy (4 colors: green monochrome)
      // Darkest to lightest green
      colors = {
        cv::Vec3b(15, 56, 15),    // Darkest green
        cv::Vec3b(48, 98, 48),    // Dark green
        cv::Vec3b(139, 172, 15),  // Light green
        cv::Vec3b(155, 188, 15)   // Lightest green
      };
      break;
    }
    
    case PalettePreset::GameBoyPocket: {
      // Game Boy Pocket (4 colors: monochrome)
      colors = {
        cv::Vec3b(15, 15, 15),    // Black
        cv::Vec3b(79, 79, 79),    // Dark gray
        cv::Vec3b(163, 163, 163), // Light gray
        cv::Vec3b(255, 255, 255)  // White
      };
      break;
    }
    
    case PalettePreset::Pico8: {
      // Pico-8 fantasy console palette (16 colors)
      // Reference: https://lospec.com/palette-list/pico-8
      colors = {
        cv::Vec3b(0, 0, 0),       // Black
        cv::Vec3b(29, 43, 83),    // Dark blue
        cv::Vec3b(126, 37, 83),   // Dark purple
        cv::Vec3b(0, 135, 81),    // Dark green
        cv::Vec3b(171, 82, 54),   // Brown
        cv::Vec3b(95, 87, 79),    // Dark gray
        cv::Vec3b(194, 195, 199), // Light gray
        cv::Vec3b(255, 241, 232), // White
        cv::Vec3b(255, 0, 77),    // Red
        cv::Vec3b(255, 163, 0),   // Orange
        cv::Vec3b(255, 236, 39),  // Yellow
        cv::Vec3b(0, 228, 54),    // Green
        cv::Vec3b(41, 173, 255),  // Blue
        cv::Vec3b(131, 118, 156), // Indigo
        cv::Vec3b(255, 119, 168), // Pink
        cv::Vec3b(255, 204, 170)  // Peach
      };
      break;
    }
    
    case PalettePreset::CGA: {
      // CGA 4-color mode palette (Cyan/Magenta/White)
      colors = {
        cv::Vec3b(0, 0, 0),       // Black
        cv::Vec3b(85, 255, 255),  // Cyan
        cv::Vec3b(255, 85, 255),  // Magenta
        cv::Vec3b(255, 255, 255)  // White
      };
      break;
    }
    
    case PalettePreset::EGA: {
      // EGA 16-color palette
      colors = {
        cv::Vec3b(0, 0, 0),       // Black
        cv::Vec3b(0, 0, 170),     // Blue
        cv::Vec3b(0, 170, 0),     // Green
        cv::Vec3b(0, 170, 170),   // Cyan
        cv::Vec3b(170, 0, 0),     // Red
        cv::Vec3b(170, 0, 170),   // Magenta
        cv::Vec3b(170, 85, 0),    // Brown
        cv::Vec3b(170, 170, 170), // Light gray
        cv::Vec3b(85, 85, 85),    // Dark gray
        cv::Vec3b(85, 85, 255),   // Bright blue
        cv::Vec3b(85, 255, 85),   // Bright green
        cv::Vec3b(85, 255, 255),  // Bright cyan
        cv::Vec3b(255, 85, 85),   // Bright red
        cv::Vec3b(255, 85, 255),  // Bright magenta
        cv::Vec3b(255, 255, 85),  // Yellow
        cv::Vec3b(255, 255, 255)  // White
      };
      break;
    }
    
    case PalettePreset::Commodore64: {
      // Commodore 64 palette (16 colors)
      colors = {
        cv::Vec3b(0, 0, 0),       // Black
        cv::Vec3b(255, 255, 255), // White
        cv::Vec3b(136, 0, 0),     // Red
        cv::Vec3b(170, 255, 238), // Cyan
        cv::Vec3b(204, 68, 204),  // Purple
        cv::Vec3b(0, 204, 85),    // Green
        cv::Vec3b(0, 0, 170),     // Blue
        cv::Vec3b(238, 238, 119), // Yellow
        cv::Vec3b(221, 136, 85),  // Orange
        cv::Vec3b(102, 68, 0),    // Brown
        cv::Vec3b(255, 119, 119), // Light red
        cv::Vec3b(51, 51, 51),    // Dark gray
        cv::Vec3b(119, 119, 119), // Medium gray
        cv::Vec3b(170, 255, 102), // Light green
        cv::Vec3b(0, 136, 255),   // Light blue
        cv::Vec3b(187, 187, 187)  // Light gray
      };
      break;
    }
    
    default:
      // Custom or unknown - return empty (should not be called)
      break;
  }
  
  return colors;
}

cv::Mat PixelArtProcessor::QuantizeWithFixedPalette(const cv::Mat& smallBgr, PalettePreset preset) {
  if (smallBgr.empty() || smallBgr.type() != CV_8UC3) return {};
  
  std::vector<cv::Vec3b> palette = GetPaletteColors(preset);
  if (palette.empty()) return {};
  
  cv::Mat result(smallBgr.size(), CV_8UC3);
  
  // For each pixel, find the closest color in the fixed palette
  for (int y = 0; y < smallBgr.rows; ++y) {
    const cv::Vec3b* srcRow = smallBgr.ptr<cv::Vec3b>(y);
    cv::Vec3b* dstRow = result.ptr<cv::Vec3b>(y);
    
    for (int x = 0; x < smallBgr.cols; ++x) {
      const cv::Vec3b& pixel = srcRow[x];
      
      // Find nearest palette color using Euclidean distance in RGB
      float minDist = std::numeric_limits<float>::max();
      int bestIdx = 0;
      
      for (size_t i = 0; i < palette.size(); ++i) {
        float dist = ColorDistanceSquared(pixel, palette[i]);
        if (dist < minDist) {
          minDist = dist;
          bestIdx = static_cast<int>(i);
        }
      }
      
      dstRow[x] = palette[bestIdx];
    }
  }
  
  return result;
}

cv::Vec3b PixelArtProcessor::FindNearestPaletteColor(const cv::Vec3b& color, const std::vector<cv::Vec3b>& palette) {
  if (palette.empty()) return color;
  
  float minDist = std::numeric_limits<float>::max();
  size_t bestIdx = 0;
  
  for (size_t i = 0; i < palette.size(); ++i) {
    float dist = ColorDistanceSquared(color, palette[i]);
    if (dist < minDist) {
      minDist = dist;
      bestIdx = i;
    }
  }
  
  return palette[bestIdx];
}

cv::Mat PixelArtProcessor::QuantizeWithFixedPaletteDither(const cv::Mat& smallBgr, PalettePreset preset) {
  if (smallBgr.empty() || smallBgr.type() != CV_8UC3) return {};
  
  std::vector<cv::Vec3b> palette = GetPaletteColors(preset);
  if (palette.empty()) return {};
  
  // Work on a copy to allow in-place modification during dithering
  cv::Mat work = smallBgr.clone();
  
  // Floyd-Steinberg dithering: scan left-to-right, top-to-bottom
  // Error distribution weights:
  //    X   7/16
  // 3/16 5/16 1/16
  const float w_right = 7.0f / 16.0f;
  const float w_down_left = 3.0f / 16.0f;
  const float w_down = 5.0f / 16.0f;
  const float w_down_right = 1.0f / 16.0f;
  
  for (int y = 0; y < work.rows; ++y) {
    cv::Vec3b* row = work.ptr<cv::Vec3b>(y);
    
    for (int x = 0; x < work.cols; ++x) {
      cv::Vec3b& oldPixel = row[x];
      
      // Clamp pixel values to valid range (may have been modified by error diffusion)
      oldPixel[0] = static_cast<uchar>(ClampInt(static_cast<int>(oldPixel[0]), 0, 255));
      oldPixel[1] = static_cast<uchar>(ClampInt(static_cast<int>(oldPixel[1]), 0, 255));
      oldPixel[2] = static_cast<uchar>(ClampInt(static_cast<int>(oldPixel[2]), 0, 255));
      
      // Find nearest palette color
      cv::Vec3b newPixel = FindNearestPaletteColor(oldPixel, palette);
      
      // Calculate quantization error
      cv::Vec3f error(
          static_cast<float>(oldPixel[0]) - static_cast<float>(newPixel[0]),
          static_cast<float>(oldPixel[1]) - static_cast<float>(newPixel[1]),
          static_cast<float>(oldPixel[2]) - static_cast<float>(newPixel[2]));
      
      // Replace current pixel with quantized color
      oldPixel = newPixel;
      
      // Distribute error to neighboring pixels (Floyd-Steinberg pattern)
      if (x + 1 < work.cols) {
        // Right neighbor
        cv::Vec3b& right = row[x + 1];
        right[0] = static_cast<uchar>(ClampInt(static_cast<int>(right[0] + error[0] * w_right), 0, 255));
        right[1] = static_cast<uchar>(ClampInt(static_cast<int>(right[1] + error[1] * w_right), 0, 255));
        right[2] = static_cast<uchar>(ClampInt(static_cast<int>(right[2] + error[2] * w_right), 0, 255));
      }
      
      if (y + 1 < work.rows) {
        cv::Vec3b* nextRow = work.ptr<cv::Vec3b>(y + 1);
        
        if (x - 1 >= 0) {
          // Bottom-left neighbor
          cv::Vec3b& downLeft = nextRow[x - 1];
          downLeft[0] = static_cast<uchar>(ClampInt(static_cast<int>(downLeft[0] + error[0] * w_down_left), 0, 255));
          downLeft[1] = static_cast<uchar>(ClampInt(static_cast<int>(downLeft[1] + error[1] * w_down_left), 0, 255));
          downLeft[2] = static_cast<uchar>(ClampInt(static_cast<int>(downLeft[2] + error[2] * w_down_left), 0, 255));
        }
        
        // Bottom neighbor
        cv::Vec3b& down = nextRow[x];
        down[0] = static_cast<uchar>(ClampInt(static_cast<int>(down[0] + error[0] * w_down), 0, 255));
        down[1] = static_cast<uchar>(ClampInt(static_cast<int>(down[1] + error[1] * w_down), 0, 255));
        down[2] = static_cast<uchar>(ClampInt(static_cast<int>(down[2] + error[2] * w_down), 0, 255));
        
        if (x + 1 < work.cols) {
          // Bottom-right neighbor
          cv::Vec3b& downRight = nextRow[x + 1];
          downRight[0] = static_cast<uchar>(ClampInt(static_cast<int>(downRight[0] + error[0] * w_down_right), 0, 255));
          downRight[1] = static_cast<uchar>(ClampInt(static_cast<int>(downRight[1] + error[1] * w_down_right), 0, 255));
          downRight[2] = static_cast<uchar>(ClampInt(static_cast<int>(downRight[2] + error[2] * w_down_right), 0, 255));
        }
      }
    }
  }
  
  return work;
}

cv::Mat PixelArtProcessor::QuantizeWithKMeansLabDither(const cv::Mat& smallBgr, int paletteSize) {
  if (smallBgr.empty() || smallBgr.type() != CV_8UC3) return {};
  
  const int total = smallBgr.rows * smallBgr.cols;
  if (total <= 0) return {};
  
  paletteSize = std::max(2, std::min(paletteSize, total));
  
  // Step 1: Build palette using K-means (same as non-dither version)
  cv::Mat smallLab;
  cv::cvtColor(smallBgr, smallLab, cv::COLOR_BGR2Lab);
  
  cv::Mat samples(total, 3, CV_32F);
  for (int y = 0; y < smallLab.rows; ++y) {
    const cv::Vec3b* row = smallLab.ptr<cv::Vec3b>(y);
    for (int x = 0; x < smallLab.cols; ++x) {
      const int i = y * smallLab.cols + x;
      samples.at<float>(i, 0) = static_cast<float>(row[x][0]);
      samples.at<float>(i, 1) = static_cast<float>(row[x][1]);
      samples.at<float>(i, 2) = static_cast<float>(row[x][2]);
    }
  }
  
  cv::Mat labels;
  cv::Mat centers;
  const int attempts = 3;
  cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1.0);
  cv::kmeans(samples, paletteSize, labels, criteria, attempts, cv::KMEANS_PP_CENTERS, centers);
  
  // Convert cluster centers from Lab to BGR to build palette
  std::vector<cv::Vec3b> palette;
  palette.reserve(paletteSize);
  for (int i = 0; i < paletteSize; ++i) {
    cv::Mat labPixel(1, 1, CV_8UC3);
    labPixel.at<cv::Vec3b>(0, 0) = cv::Vec3b(
        static_cast<uchar>(ClampInt(static_cast<int>(std::lround(centers.at<float>(i, 0))), 0, 255)),
        static_cast<uchar>(ClampInt(static_cast<int>(std::lround(centers.at<float>(i, 1))), 0, 255)),
        static_cast<uchar>(ClampInt(static_cast<int>(std::lround(centers.at<float>(i, 2))), 0, 255)));
    
    cv::Mat bgrPixel;
    cv::cvtColor(labPixel, bgrPixel, cv::COLOR_Lab2BGR);
    palette.push_back(bgrPixel.at<cv::Vec3b>(0, 0));
  }
  
  // Step 2: Apply Floyd-Steinberg dithering using the K-means palette
  cv::Mat work = smallBgr.clone();
  
  const float w_right = 7.0f / 16.0f;
  const float w_down_left = 3.0f / 16.0f;
  const float w_down = 5.0f / 16.0f;
  const float w_down_right = 1.0f / 16.0f;
  
  for (int y = 0; y < work.rows; ++y) {
    cv::Vec3b* row = work.ptr<cv::Vec3b>(y);
    
    for (int x = 0; x < work.cols; ++x) {
      cv::Vec3b& oldPixel = row[x];
      
      // Clamp pixel values
      oldPixel[0] = static_cast<uchar>(ClampInt(static_cast<int>(oldPixel[0]), 0, 255));
      oldPixel[1] = static_cast<uchar>(ClampInt(static_cast<int>(oldPixel[1]), 0, 255));
      oldPixel[2] = static_cast<uchar>(ClampInt(static_cast<int>(oldPixel[2]), 0, 255));
      
      // Find nearest palette color
      cv::Vec3b newPixel = FindNearestPaletteColor(oldPixel, palette);
      
      // Calculate quantization error
      cv::Vec3f error(
          static_cast<float>(oldPixel[0]) - static_cast<float>(newPixel[0]),
          static_cast<float>(oldPixel[1]) - static_cast<float>(newPixel[1]),
          static_cast<float>(oldPixel[2]) - static_cast<float>(newPixel[2]));
      
      // Replace current pixel with quantized color
      oldPixel = newPixel;
      
      // Distribute error to neighboring pixels
      if (x + 1 < work.cols) {
        cv::Vec3b& right = row[x + 1];
        right[0] = static_cast<uchar>(ClampInt(static_cast<int>(right[0] + error[0] * w_right), 0, 255));
        right[1] = static_cast<uchar>(ClampInt(static_cast<int>(right[1] + error[1] * w_right), 0, 255));
        right[2] = static_cast<uchar>(ClampInt(static_cast<int>(right[2] + error[2] * w_right), 0, 255));
      }
      
      if (y + 1 < work.rows) {
        cv::Vec3b* nextRow = work.ptr<cv::Vec3b>(y + 1);
        
        if (x - 1 >= 0) {
          cv::Vec3b& downLeft = nextRow[x - 1];
          downLeft[0] = static_cast<uchar>(ClampInt(static_cast<int>(downLeft[0] + error[0] * w_down_left), 0, 255));
          downLeft[1] = static_cast<uchar>(ClampInt(static_cast<int>(downLeft[1] + error[1] * w_down_left), 0, 255));
          downLeft[2] = static_cast<uchar>(ClampInt(static_cast<int>(downLeft[2] + error[2] * w_down_left), 0, 255));
        }
        
        cv::Vec3b& down = nextRow[x];
        down[0] = static_cast<uchar>(ClampInt(static_cast<int>(down[0] + error[0] * w_down), 0, 255));
        down[1] = static_cast<uchar>(ClampInt(static_cast<int>(down[1] + error[1] * w_down), 0, 255));
        down[2] = static_cast<uchar>(ClampInt(static_cast<int>(down[2] + error[2] * w_down), 0, 255));
        
        if (x + 1 < work.cols) {
          cv::Vec3b& downRight = nextRow[x + 1];
          downRight[0] = static_cast<uchar>(ClampInt(static_cast<int>(downRight[0] + error[0] * w_down_right), 0, 255));
          downRight[1] = static_cast<uchar>(ClampInt(static_cast<int>(downRight[1] + error[1] * w_down_right), 0, 255));
          downRight[2] = static_cast<uchar>(ClampInt(static_cast<int>(downRight[2] + error[2] * w_down_right), 0, 255));
        }
      }
    }
  }
  
  return work;
}

void PixelArtProcessor::ApplyPixelArtOutline(cv::Mat& bgr, int thickness) {
  if (bgr.empty() || bgr.type() != CV_8UC3) return;
  thickness = std::max(1, std::min(thickness, 5));

  // Create edge mask by detecting significant color differences
  // For pixel art, we detect edges where adjacent pixels have large color differences
  cv::Mat edges(bgr.size(), CV_8UC1, cv::Scalar(0));
  
  // Threshold for considering two colors "different" enough to draw an outline
  // Using both luminance and color distance for better detection
  const int luminanceThreshold = 35; // Luminance difference threshold
  const int colorDistanceThreshold = 40; // RGB color distance threshold
  
  for (int y = 0; y < bgr.rows; ++y) {
    const cv::Vec3b* row = bgr.ptr<cv::Vec3b>(y);
    uchar* edgeRow = edges.ptr<uchar>(y);
    
    for (int x = 0; x < bgr.cols; ++x) {
      const cv::Vec3b& current = row[x];
      
      // Calculate luminance (perceptual luminance: 0.299*R + 0.587*G + 0.114*B)
      int lumCurrent = static_cast<int>(
          0.299f * current[2] + 0.587f * current[1] + 0.114f * current[0]);
      
      // Check 4-connected neighbors (up, down, left, right)
      // If any neighbor is significantly different, mark as edge
      bool isEdge = false;
      
      // Right neighbor
      if (x + 1 < bgr.cols) {
        const cv::Vec3b& neighbor = row[x + 1];
        int lumNeighbor = static_cast<int>(
            0.299f * neighbor[2] + 0.587f * neighbor[1] + 0.114f * neighbor[0]);
        
        // Check both luminance and color distance
        int lumDiff = std::abs(lumCurrent - lumNeighbor);
        float colorDist = std::sqrt(ColorDistanceSquared(current, neighbor));
        
        if (lumDiff >= luminanceThreshold || colorDist >= colorDistanceThreshold) {
          isEdge = true;
        }
      }
      
      // Bottom neighbor
      if (!isEdge && y + 1 < bgr.rows) {
        const cv::Vec3b* nextRow = bgr.ptr<cv::Vec3b>(y + 1);
        const cv::Vec3b& neighbor = nextRow[x];
        int lumNeighbor = static_cast<int>(
            0.299f * neighbor[2] + 0.587f * neighbor[1] + 0.114f * neighbor[0]);
        
        int lumDiff = std::abs(lumCurrent - lumNeighbor);
        float colorDist = std::sqrt(ColorDistanceSquared(current, neighbor));
        
        if (lumDiff >= luminanceThreshold || colorDist >= colorDistanceThreshold) {
          isEdge = true;
        }
      }
      
      // Left neighbor
      if (!isEdge && x > 0) {
        const cv::Vec3b& neighbor = row[x - 1];
        int lumNeighbor = static_cast<int>(
            0.299f * neighbor[2] + 0.587f * neighbor[1] + 0.114f * neighbor[0]);
        
        int lumDiff = std::abs(lumCurrent - lumNeighbor);
        float colorDist = std::sqrt(ColorDistanceSquared(current, neighbor));
        
        if (lumDiff >= luminanceThreshold || colorDist >= colorDistanceThreshold) {
          isEdge = true;
        }
      }
      
      // Top neighbor
      if (!isEdge && y > 0) {
        const cv::Vec3b* prevRow = bgr.ptr<cv::Vec3b>(y - 1);
        const cv::Vec3b& neighbor = prevRow[x];
        int lumNeighbor = static_cast<int>(
            0.299f * neighbor[2] + 0.587f * neighbor[1] + 0.114f * neighbor[0]);
        
        int lumDiff = std::abs(lumCurrent - lumNeighbor);
        float colorDist = std::sqrt(ColorDistanceSquared(current, neighbor));
        
        if (lumDiff >= luminanceThreshold || colorDist >= colorDistanceThreshold) {
          isEdge = true;
        }
      }
      
      edgeRow[x] = isEdge ? 255 : 0;
    }
  }
  
  // Apply morphology operations based on thickness
  if (thickness == 1) {
    // Keep single-pixel edges - apply slight erosion to remove noise
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));
    cv::morphologyEx(edges, edges, cv::MORPH_OPEN, kernel);
  } else {
    // Thicken edges for thicker outlines using dilation
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * thickness + 1, 2 * thickness + 1));
    cv::dilate(edges, edges, kernel);
  }
  
  // Draw dark outlines on the original image
  // Use adaptive darkening: darken pixels at edge locations
  for (int y = 0; y < bgr.rows; ++y) {
    cv::Vec3b* row = bgr.ptr<cv::Vec3b>(y);
    const uchar* edgeRow = edges.ptr<uchar>(y);
    
    for (int x = 0; x < bgr.cols; ++x) {
      if (edgeRow[x] > 128) {
        // Edge pixel: draw dark outline
        cv::Vec3b& pixel = row[x];
        
        // Adaptive darkening: darken by 50-70% depending on original brightness
        // This creates more natural-looking outlines that adapt to the pixel color
        int darkenAmount = 70; // Base darkening amount
        
        // Calculate brightness of original pixel
        int brightness = (static_cast<int>(pixel[0]) + static_cast<int>(pixel[1]) + static_cast<int>(pixel[2])) / 3;
        
        // Adjust darkening based on brightness (darker pixels get less darkening)
        if (brightness < 64) {
          darkenAmount = 40; // Less darkening for already dark pixels
        } else if (brightness > 192) {
          darkenAmount = 90; // More darkening for bright pixels
        }
        
        pixel[0] = static_cast<uchar>(std::max(0, pixel[0] - darkenAmount)); // B
        pixel[1] = static_cast<uchar>(std::max(0, pixel[1] - darkenAmount)); // G
        pixel[2] = static_cast<uchar>(std::max(0, pixel[2] - darkenAmount)); // R
        
        // Alternative: Use pure black outline (uncomment to use)
        // row[x] = cv::Vec3b(0, 0, 0);
      }
    }
  }
}


