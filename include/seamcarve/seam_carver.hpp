#pragma once
#include <atomic>
#include <functional>
#include <stdexcept>
#include <vector>
#include <opencv2/core.hpp>
#include "seamcarve/types.hpp"
#include "seamcarve/energy.hpp"

namespace seamcarve {
    /// One removed seam: which way it ran and where it was in the image at the moment it was removed.
    struct SeamStep {
        Orientation orientation{ Orientation::VERTICAL };
        Seam seam;  // one column per row (vertical) or one row per column (horizontal)
    };

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

        // Optional hooks for watching a carve step by step (the GUI uses them; the CLI leaves them empty).
        // onSeamFound is called on the thread that runs Carve(), after a seam is found and before it is removed.
        // `image` is the image the seam was found in, in the orientation it is carved in: during the height pass it is
        // transposed, so seam.size() == image.rows in both cases. It is only valid during the call; copy it to keep it.
        std::function<void(const cv::Mat& image, const Seam& seam, Orientation orientation)> onSeamFound;
        std::vector<SeamStep>* seamLog{ nullptr };  // when set, Carve() appends every removed seam here, in order
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

    /**
    * @brief Paints a seam onto an image (used to show which pixels are about to be removed).
    * @param[in,out] image	- 8-bit BGR image (CV_8UC3); the seam's pixels are overwritten.
    * @param[in] seam	- One column per row (vertical) or one row per column (horizontal).
    * @param[in] orientation	- Whether the seam is vertical or horizontal.
    * @param[in] colour	- BGR colour to paint with (default red).
    * @param[in] thickness	- How many pixels wide the line is (at least 1); extra pixels go right of / below the seam.
    * @throws std::invalid_argument If the image is not CV_8UC3 or the seam length does not match the image.
    * @throws std::out_of_range If a seam index is outside the image.
    */
    void PaintSeam(cv::Mat& image, const Seam& seam, Orientation orientation, const cv::Scalar& colour = cv::Scalar(0, 0, 255), int thickness = 1);

    /**
    * @brief Works out, for every pixel of the original image, in which step it was removed.
    * The seams in a SeamStep are in the coordinates of the image at that moment, so they cannot be drawn on
    * the original directly. This replays the removals on a map of original positions to translate them.
    * @param[in] originalSize	- Size of the image the carve started with.
    * @param[in] steps	- The removed seams in order (CarveOptions::seamLog).
    * @return A CV_32S matrix of originalSize: the index of the step that removed that pixel, or -1 if it is still in the result.
    * @throws std::invalid_argument If a seam does not fit the image it should be removed from.
    * @throws std::out_of_range If a seam index is outside the image.
    */
    cv::Mat BuildRemovalMap(cv::Size originalSize, const std::vector<SeamStep>& steps);

    /**
    * @brief Draws every removed seam on a copy of the original image, coloured by order (first seam blue, last red).
    * @param[in] original	- The image the carve started with (CV_8UC3).
    * @param[in] removalMap	- Result of BuildRemovalMap for that image.
    * @param[in] stepCount	- Number of steps in the carve (spreads the colours over the steps).
    * @return A copy of the original with the removed pixels coloured.
    * @throws std::invalid_argument If the sizes or types do not match.
    */
    cv::Mat RenderSeamOverlay(const cv::Mat& original, const cv::Mat& removalMap, int stepCount);

    /**
    * @brief Shows the carve as it was after some of its steps: the first stepIndex seams are removed and the
    * next one (if there is one) is painted red. stepIndex == steps.size() gives the final image without a seam.
    * @param[in] original	- The image the carve started with (CV_8UC3).
    * @param[in] steps	- The removed seams in order (CarveOptions::seamLog).
    * @param[in] stepIndex	- Number of seams already removed, from 0 to steps.size().
    * @param[in] seamThickness	- Width in pixels of the painted seam.
    * @return The image at that step.
    * @throws std::out_of_range If stepIndex is outside [0, steps.size()].
    */
    cv::Mat RenderCarveStep(const cv::Mat& original, const std::vector<SeamStep>& steps, int stepIndex, int seamThickness = 2);

}  // namespace seamcarve