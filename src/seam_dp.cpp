#include "seamcarve/seam_finders.hpp"
#include <stdexcept>
#include <algorithm>

namespace seamcarve {
    Seam FindSeamDP(const cv::Mat& energy) {
        if (energy.empty() || energy.type() != CV_64F)
            throw std::invalid_argument("FindSeamDP: energy must be a non-empty CV_64F matrix");

        const int rows{ energy.rows }, cols{ energy.cols };
        cv::Mat cost(rows, cols, CV_64F);
        cv::Mat from(rows, cols, CV_8S, cv::Scalar(0));

        // for the first row, as there no pixels above, the cost is just the energy
        energy.row(0).copyTo(cost.row(0));

        for (int row{ 1 }; row < rows; ++row) {
            const auto *energyRow{ energy.ptr<double>(row) },
                        *aboveRow{ cost.ptr<double>(row - 1) };
            auto* costRow{ cost.ptr<double>(row) };
            auto* fromRow{ from.ptr<schar>(row) };

            for (int col{}; col < cols; ++col) {    // pick the cheapest from the 3 pixel above col.
                int best{ col };                    // start with the pixel thats straight above
                if (col > 0 && aboveRow[col - 1] < aboveRow[best]) best = col - 1;
                if (col < cols - 1 && aboveRow[col + 1] < aboveRow[best]) best = col + 1;

                costRow[col] = energyRow[col] + aboveRow[best];
                fromRow[col] = static_cast<schar>(best - col);
            }
        }
        // after the loop, the cheapest seam lies at the pixel with the cheapest cost
        const auto* lastRow{ cost.ptr<double>(rows - 1) };
        int col { static_cast<int>(std::min_element(lastRow, lastRow + cols) - lastRow) };

        // back trace up by followin the stored offset
        Seam seam(rows);
        for (int row{ rows - 1 }; row >= 0; --row) {
            seam[row] = col;
            col += from.at<schar>(row, col);
        }
        return seam;
    }
}  // namespace seamcarve
