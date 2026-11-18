#pragma once
#include <opencv2/core.hpp>
#include "seamcarve/types.hpp"

namespace seamcarve {
    inline constexpr int DEFAULT_BASE_SIZE{ 64 };   // Block size in pixels below which the divide-and-conquer method stops splitting.

    // Every energy function has the same contract:
    //   input : 8-bit BGR image (CV_8UC3)
    //   output: single-channel CV_64F matrix, same size as the input, no negative values
    // A higher value means the pixel matters more, so a seam should avoid it.
    // C++ overloads need different parameter lists, so the approaches are selected with an
    // EnergyMethod argument and each one has its own named function that can be called or tested directly.

    /**
    * @brief Computes the per-pixel energy map with the chosen method.
    * @param[in] image	- 8-bit BGR image (CV_8UC3).
    * @param[in] method	- Which approach to use. (default to Sobel)
    * @return Single-channel CV_64F matrix, same size as the input.
    * @throws std::invalid_argument if image is invalid or if the method is not recognised.
    */
    cv::Mat ComputeEnergy(const cv::Mat& image, EnergyMethod method = EnergyMethod::SOBEL);

    /**
    * @brief Energy from finite differences between neighbouring pixels, |dI/dx| + |dI/dy|.
    * @param[in] image	- 8-bit BGR image (CV_8UC3).
    * @return Single-channel CV_64F matrix, same size as the input.
    */
    cv::Mat ComputeEnergyGradient(const cv::Mat& image);

    /**
    * @brief Energy from the Sobel filter (a 3x3 convolution) in x and y.
    * @param[in] image	- 8-bit BGR image (CV_8UC3).
    * @return Single-channel CV_64F matrix, same size as the input.
    */
    cv::Mat ComputeEnergySobel(const cv::Mat& image);

    /**
    * @brief Divide and conquer: splits the image into blocks until they are at most baseSize
    * pixels, computes the energy of each block and merges the results (Question 2b).
    * The result should match the non-split method used for the blocks, including along the cuts.
    * @param[in] image	    - 8-bit BGR image (CV_8UC3).
    * @param[in] baseSize	- Blocks no larger than this (in both directions) are computed directly.
    * @return Single-channel CV_64F matrix, same size as the input.
    */
    cv::Mat ComputeEnergyDivideAndConquer(const cv::Mat& image, int baseSize = DEFAULT_BASE_SIZE);
}  // namespace seamcarve
