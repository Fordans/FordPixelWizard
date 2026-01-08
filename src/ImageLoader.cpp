#include "ImageLoader.h"

#include <opencv2/imgcodecs.hpp>

bool ImageLoader::LoadBGR(const std::string& path, cv::Mat& outBgr, std::string& outError) {
  outError.clear();
  outBgr.release();

  // IMREAD_COLOR => always 8-bit BGR, which is what our processor expects.
  cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
  if (img.empty()) {
    outError = "Failed to load image (empty). Check path and supported formats (png/jpg).";
    return false;
  }
  outBgr = img;
  return true;
}

bool ImageLoader::Save(const std::string& path, const cv::Mat& image, std::string& outError) {
  outError.clear();
  if (image.empty()) {
    outError = "Nothing to save (image is empty).";
    return false;
  }
  try {
    if (!cv::imwrite(path, image)) {
      outError = "cv::imwrite returned false. Check file extension and output path.";
      return false;
    }
  } catch (const cv::Exception& e) {
    outError = std::string("OpenCV error: ") + e.what();
    return false;
  }
  return true;
}


