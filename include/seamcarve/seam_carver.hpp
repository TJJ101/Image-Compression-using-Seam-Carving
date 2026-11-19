#pragma once
#include <atomic>
#include <stdexcept>
#include <opencv2/core.hpp>
#include "seamcarve/types.hpp"
#include "seamcarve/energy.hpp"

namespace seamcarve {
    struct CarveOptions {
        Algorithm algorithm { Algorithm::DYNAMIC_PROGRAMMING };
        EnergyMethod energyMethod{ EnergyMethod::SOBEL };  // how the energy map is computed
        DivideAndConquer divideAndConquerType{ DivideAndConquer::RECURSION };  // only used with EnergyMethod::DIVIDE_AND_CONQUER
        int baseSize{ DEFAULT_BASE_SIZE };  // block size at which divide and conquer stops splitting
        bool verbose{ false };  // print per-seam progress / timing

        // Optional hooks for running Carve() on a background thread (the GUI does this). Both may be
        // null (the CLI leaves them null). The caller owns them and must keep them alive until Carve() returns.
        std::atomic<int>* seamsDone{ nullptr };      // Carve() adds 1 after removing each seam (progress bar)
        const std::atomic<bool>* cancel{ nullptr };  // when set, Carve() stops and throws CarveCancelled
    };

    /// Thrown by Carve() when CarveOptions::cancel is set while it is running.
    struct CarveCancelled : std::runtime_error {
        CarveCancelled() : std::runtime_error{ "Carve cancelled" } {}
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