#pragma once
#include <opencv2/core.hpp>

namespace seamcarve {
    /**
    * @brief Computes the per-pixel energy map (e.g. gradient magnitude / Sobel).
    * Task 1a.
    * @param[in] image	- 8-bit BGR image (CV_8UC3).
    * @return Single-channel CV_64F matrix, same size as the input.
    */
    cv::Mat ComputeEnergy(const cv::Mat& image);

}  // namespace seamcarve