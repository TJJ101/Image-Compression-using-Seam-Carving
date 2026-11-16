#include <stdexcept>
#include "seamcarve/seam_finders.hpp"

namespace seamcarve {
    Seam FindSeamGreedy(const cv::Mat& /*energy*/) {
        // TODO(Task 3): pick min-energy start pixel, then the cheapest of the 3 neighbours each row.
        throw std::logic_error("FindSeamGreedy: not implemented");
    }
}  // namespace seamcarve
