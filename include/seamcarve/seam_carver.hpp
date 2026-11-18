#pragma once
#include <opencv2/core.hpp>
#include "seamcarve/types.hpp"

namespace seamcarve {
    struct CarveOptions {
        Algorithm algorithm { Algorithm::DYNAMIC_PROGRAMMING };
        EnergyMethod energyMethod{ EnergyMethod::SOBEL };  // how the energy map is computed
        bool verbose{ false };  // print per-seam progress / timing
    };

    /**
    * @brief Removes one seam from the image.
    * Task 1c.
    * @param[in] image	- Image to remove the seam from.
    * @param[in] seam	- Seam to remove: one index per row (vertical) or per column (horizontal).
    * @param[in] orientation	- Whether the seam is vertical or horizontal.
    * @return A copy of the image, one pixel narrower (vertical) or shorter (horizontal).
    */
    cv::Mat RemoveSeam(const cv::Mat& image, const Seam& seam, Orientation orientation);

    /**
    * @brief Repeatedly finds and removes the minimum seam until the target size is reached.
    * Only shrinking is required by the assignment.
    * @param[in] image	- Input image.
    * @param[in] targetWidth	- Target width in pixels.
    * @param[in] targetHeight	- Target height in pixels.
    * @param[in] options	- Algorithm, energy method and verbosity.
    * @return The resized image.
    */
    cv::Mat Carve(const cv::Mat& image, int targetWidth, int targetHeight, const CarveOptions& options);

}  // namespace seamcarve