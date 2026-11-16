#include "seamcarve/energy.hpp"
#include <stdexcept>

namespace seamcarve {
    cv::Mat ComputeEnergy(const cv::Mat& /*image*/) {
        // TODO(Task 1a): gradient magnitude / Sobel energy map.
        throw std::logic_error("ComputeEnergy: not implemented");
    }
}  // namespace seamcarve
