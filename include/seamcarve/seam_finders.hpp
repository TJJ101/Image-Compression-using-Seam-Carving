#pragma once
#include <opencv2/core.hpp>
#include "seamcarve/types.hpp"

namespace seamcarve {
    // Every finder takes an energy map (CV_64F) and returns the minimum VERTICAL seam
    // (one column index per row). Horizontal seams are obtained by transposing the
    // energy map before and the result after, so each algorithm is written once.

    /**
    * @brief Finds the minimum-energy vertical seam using dynamic programming (optimal).
    * Task 1b, implemented in src/seam_dp.cpp.
    * @param[in] energy	- Energy map (CV_64F).
    * @return One column index per row.
    */
    Seam FindSeamDP(const cv::Mat& energy);

    /**
    * @brief Finds a vertical seam using a greedy row-by-row choice.
    * Task 3, implemented in src/seam_greedy.cpp.
    * @param[in] energy	- Energy map (CV_64F).
    * @return One column index per row.
    */
    Seam FindSeamGreedy(const cv::Mat& energy);

    /**
    * @brief Finds the minimum-energy vertical seam as a minimum s-t cut (max-flow).
    * Task 4, implemented in src/seam_graphcut.cpp.
    * @param[in] energy	- Energy map (CV_64F).
    * @return One column index per row.
    */
    Seam FindSeamGraphCut(const cv::Mat& energy);
}  // namespace seamcarve
