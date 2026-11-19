#pragma once
#include <opencv2/core.hpp>
#include "seamcarve/types.hpp"

namespace seamcarve {
    // Every finder takes an energy map (CV_64F) and returns the minimum VERTICAL seam
    // (one column index per row). Horizontal seams are obtained by transposing the
    // energy map before and the result after, so each algorithm is written once.

    /**
    * @brief Finds the minimum-energy vertical seam using dynamic programming (optimal).
    * This is the default entry point: it calls FindSeamDPTwoRows. FindSeamDPFullTable returns exactly
    * the same seam with more memory; both have the same signature so they can be benchmarked side by side.
    * Task 1b, implemented in src/seam_dp.cpp.
    * @param[in] energy	        - Energy map (CV_64F).
    * @param[in] useFullTable   - Whether to keep the full DP table or only keep what's needed (two rows). Defaulted to false.
    * @return One column index per row.
    */
    Seam FindSeamDP(const cv::Mat& energy, bool useFullTable = false);

    /**
    * @brief Finds a vertical seam using a greedy row-by-row choice.
    * Task 3, implemented in src/seam_greedy.cpp.
    * @param[in] energy	- Energy map (CV_64F).
    * @param[in] seed	- Randomness seed, for consistency in benchmarking. 0 = random.
    * @return One column index per row.
    */
    Seam FindSeamGreedy(const cv::Mat& energy, unsigned seed = 0);

    /**
    * @brief Calls FindSeamsGreedy without setting the seed, while maintaning the same function 
    *        signature to make it easier to port for testing.
    * @param[in] energy	- Energy map (CV_64F).
    * @return One column index per row.
    */
    inline Seam FindSeamGreedyDefault(const cv::Mat& energy) { return FindSeamGreedy(energy); }

    /**
    * @brief Finds the minimum-energy vertical seam as a minimum s-t cut (max-flow).
    * Task 4, implemented in src/seam_graphcut.cpp.
    * @param[in] energy	- Energy map (CV_64F).
    * @return One column index per row.
    */
    Seam FindSeamGraphCut(const cv::Mat& energy);
}  // namespace seamcarve
