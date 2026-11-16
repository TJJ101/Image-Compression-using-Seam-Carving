#include <stdexcept>
#include "seamcarve/seam_finders.hpp"

namespace seamcarve {
    Seam FindSeamDP(const cv::Mat& /*energy*/) {
        // TODO(Task 1b): cumulative-minimum-energy DP table + backtracking.
        throw std::logic_error("FindSeamDP: not implemented");
    }
}  // namespace seamcarve
