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
        // At the start of every iteration, stop if the caller asked to (the GUI's Cancel button):
        //     if (options.cancel && options.cancel->load()) throw CarveCancelled{};
        // and after each seam is removed, report progress (the GUI's progress bar):
        //     if (options.seamsDone) ++*options.seamsDone;
        throw std::logic_error("Carve: not implemented");
    }
}  // namespace seamcarve
