#pragma once

#include <opencv2/core.hpp>
#include <string>

// ImageLoader: minimal, UI-agnostic image I/O wrapper.
// Why wrap OpenCV?
// - Keeps UI code clean and testable.
// - Centralizes any future format conversions / metadata handling.
class ImageLoader {
public:
  // Loads image as 8-bit BGR (OpenCV default), returns true on success.
  static bool LoadBGR(const std::string& path, cv::Mat& outBgr, std::string& outError);

  // Saves BGR/RGBA/Gray images using OpenCV imwrite.
  static bool Save(const std::string& path, const cv::Mat& image, std::string& outError);
};


