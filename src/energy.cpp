#include "seamcarve/energy.hpp"
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace seamcarve {
    cv::Mat ComputeEnergy(const cv::Mat& image, EnergyMethod method) {
        if (image.empty() || image.type() != CV_8UC3)
            throw std::invalid_argument("Image is empty or isn't 8-bit, 3 channel BGR");

        switch (method) {
            case EnergyMethod::GRADIENT: return ComputeEnergyGradient(image);
            case EnergyMethod::SOBEL: return ComputeEnergySobel(image);
            case EnergyMethod::DIVIDE_AND_CONQUER: return ComputeEnergyDivideAndConquer(image, DEFAULT_BASE_SIZE);
        }
        throw std::invalid_argument("ComputeEnergy: unknown energy method");
    }

    cv::Mat ComputeEnergyGradient(const cv::Mat& /*image*/) {
        // TODO(Task 1a): finite differences.
        //  1. Convert to grayscale (or handle the 3 channels and combine them).
        //  2. dx = I(x+1, y) - I(x-1, y), dy = I(x, y+1) - I(x, y-1).
        //  3. energy = |dx| + |dy|.
        //  4. Decide and document how the first and last rows/columns are handled.
        //  5. Return a CV_64F matrix

        throw std::logic_error("ComputeEnergyGradient: not implemented");
    }

    cv::Mat ComputeEnergySobel(const cv::Mat& image) {
        cv::Mat grayImg;
        cv::cvtColor(image, grayImg, cv::COLOR_BGR2GRAY);  // convert image to grayscale

        cv::Mat dx, dy;
        cv::Sobel(grayImg, dx, CV_64F, 1, 0);
        cv::Sobel(grayImg, dy, CV_64F, 0, 1);
       
        return cv::abs(dx) + cv::abs(dy);   // calculate energy using |dx| + |dy|
    }

    cv::Mat ComputeEnergyDivideAndConquer(const cv::Mat& /*image*/, int /*baseSize*/) {
        // TODO(Question 2b): divide and conquer.
        //  1. If the block is at most baseSize in both directions, compute it directly (Sobel or gradient).
        //  2. Otherwise split it in two, recurse on each half and merge into one result.
        //  3. A pixel next to a cut needs its neighbour from the other half: give every block a 1-pixel
        //     overlap (a "halo") so the merged result equals the unsplit computation.
        //  4. Compare running time against the direct methods; the halves are independent, so this is also
        //     where multithreading (analysis 5d) could be tried.
        throw std::logic_error("ComputeEnergyDivideAndConquer: not implemented");
    }
}  // namespace seamcarve
