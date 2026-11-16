#include "seamcarve/seam_carver.hpp"
#include <stdexcept>

namespace seamcarve {
    cv::Mat RemoveSeam(const cv::Mat& /*image*/, const Seam& /*seam*/, Orientation /*orientation*/) {
        // TODO(Task 1c): copy every pixel except the seam pixel in each row/column.
        throw std::logic_error("RemoveSeam: not implemented");
    }

    cv::Mat Carve(const cv::Mat& /*image*/, int /*targetWidth*/, int /*targetHeight*/, const CarveOptions& /*options*/) {
        // TODO(Task 1c): loop { ComputeEnergy -> find*Seam (by options.algorithm) -> removeSeam }
        // until the target size is reached. Handle horizontal seams by transposing.
        throw std::logic_error("Carve: not implemented");
    }
}  // namespace seamcarve
