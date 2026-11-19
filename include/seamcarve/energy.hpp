#pragma once
#include <opencv2/core.hpp>
#include "seamcarve/types.hpp"

namespace seamcarve {
    inline constexpr int DEFAULT_BASE_SIZE{ 32 };   // Block size in pixels below which the divide-and-conquer method stops splitting.

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
    * @param[in] type   - What divide & conquer method to use (only for EnergyMethod::DIVIDE_AND_CONQUER).
    * @param[in] baseSize	- Block size where divide and conquer stops splitting (only for EnergyMethod::DIVIDE_AND_CONQUER).
    * @return Single-channel CV_64F matrix, same size as the input.
    * @throws std::invalid_argument if image is invalid or if the method is not recognised.
    */
    cv::Mat ComputeEnergy(const cv::Mat& image, EnergyMethod method = EnergyMethod::SOBEL, DivideAndConquer type = DivideAndConquer::RECURSION, int baseSize = DEFAULT_BASE_SIZE);

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
    * @param[in] type       - What approach to use, recursion or iteration.
    * @param[in] baseSize	- Blocks no larger than this (in both directions) are computed directly.
    * @return Single-channel CV_64F matrix, same size as the input.
    */
    cv::Mat ComputeEnergyDivideAndConquer(const cv::Mat& image, DivideAndConquer type, int baseSize = DEFAULT_BASE_SIZE);

    /**
    * @brief Uses recursion to compute the energy with divide and conquer approach.
    * The result should match the non-split method used for the blocks, including along the cuts.
    * @param[in] image	    - 8-bit BGR image (CV_8UC3).
    * @param[in] baseSize	- Blocks no larger than this (in both directions) are computed directly.
    * @return Single-channel CV_64F matrix, same size as the input.
    */
    cv::Mat ComputeEnergyDivideAndConquerWithRecursion(const cv::Mat& image, int baseSize = DEFAULT_BASE_SIZE);

    /**
    * @brief Uses iteration to compute the energy with divide and conquer approach.
    * The result should match the non-split method used for the blocks, including along the cuts.
    * @param[in] image	    - 8-bit BGR image (CV_8UC3).
    * @param[in] baseSize	- Blocks no larger than this (in both directions) are computed
    * directly.
    * @return Single-channel CV_64F matrix, same size as the input.
    */
    cv::Mat ComputeEnergyDivideAndConquerWithIteration(const cv::Mat& image, int baseSize = DEFAULT_BASE_SIZE);

    /**
    * @brief Brings an energy map up to date after a vertical seam was removed from the image and from the map.
    * Removing a seam only changes the energy of the pixels within one column of it, so only that band is
    * recomputed instead of the whole map: the band is cropped out of the image (with one extra column on each
    * side for the filter to read) and the chosen method's own full-map function runs on the crop
    * (Sobel with OpenCV's Sobel, Gradient with its finite differences, Divide and conquer with its recursion
    * or iteration). The result equals a full ComputeEnergy of the image.
    * @param[in,out] energy	- Energy map (CV_64F) with the seam already removed; fixed in place.
    * @param[in] image	    - 8-bit BGR image (CV_8UC3) with the seam already removed, same size as energy.
    * @param[in] seam	    - The removed seam, one column per row of the image before the removal.
    * @param[in] method	    - Which energy method the map uses.
    * @param[in] type	    - Recursion or iteration (only for EnergyMethod::DIVIDE_AND_CONQUER).
    * @param[in] baseSize	- Block size where divide and conquer stops splitting (only for EnergyMethod::DIVIDE_AND_CONQUER).
    * @throws std::invalid_argument if energy and image differ in size or the seam does not have one entry per row.
    */
    void UpdateEnergyAfterSeamRemoval(cv::Mat& energy, const cv::Mat& image, const Seam& seam, EnergyMethod method,
                                        DivideAndConquer type = DivideAndConquer::RECURSION, int baseSize = DEFAULT_BASE_SIZE);
}  // namespace seamcarve
