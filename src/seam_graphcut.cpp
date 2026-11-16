#include <stdexcept>
#include "seamcarve/seam_finders.hpp"

namespace seamcarve {
    Seam FindSeamGraphCut(const cv::Mat& /*energy*/) {
        // TODO(Task 4): build s-t graph, run max-flow (e.g. Edmonds-Karp), BFS on residual graph, read seam.
        throw std::logic_error("FindSeamGraphCut: not implemented");
    }
}  // namespace seamcarve
