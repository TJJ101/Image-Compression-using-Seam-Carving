#include "seamcarve/seam_finders.hpp"
#include <stdexcept>
#include <algorithm>
#include <vector>

namespace seamcarve {
    namespace {
        /**
         * @brief Dynamic programming that keeps the whole cost table (one double per pixel) and one
         * byte per pixel for the backtrack.
         * @param[in] energy	- Energy map (CV_64F).
         * @return One column index per row.
         * @throws std::invalid_argument If energy is empty or not CV_64F.
         */
        Seam FindSeamDPFullTable(const cv::Mat& energy) {
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

                for (int col{}; col < cols;
                     ++col) {         // pick the cheapest from the 3 pixel above col.
                    int best{ col };  // start with the pixel thats straight above
                    if (col > 0 && aboveRow[col - 1] < aboveRow[best]) best = col - 1;
                    if (col < cols - 1 && aboveRow[col + 1] < aboveRow[best]) best = col + 1;

                    costRow[col] = energyRow[col] + aboveRow[best];
                    fromRow[col] = static_cast<schar>(best - col);
                }
            }
            // after the loop, the cheapest seam lies at the pixel with the cheapest cost
            const auto* lastRow{ cost.ptr<double>(rows - 1) };
            int col{ static_cast<int>(std::min_element(lastRow, lastRow + cols) - lastRow) };

            // back trace up by followin the stored offset
            Seam seam(rows);
            for (int row{ rows - 1 }; row >= 0; --row) {
                seam[row] = col;
                col += from.at<schar>(row, col);
            }
            return seam;
        }

        /**
         * @brief Dynamic programming that keeps only two rows of costs (the row above and the
         * current row) plus one byte per pixel for the backtrack. Same seam as FindSeamDPFullTable.
         * @param[in] energy	- Energy map (CV_64F).
         * @return One column index per row.
         * @throws std::invalid_argument If energy is empty or not CV_64F.
         */
        Seam FindSeamDPTwoRows(const cv::Mat& energy) {
            const int rows{ energy.rows }, cols{ energy.cols };

            // Only the costs of the row above and of the current row are needed, so two rows are
            // enough. The offsets still need one byte per pixel, because the backtrack walks
            // through all rows.
            std::vector<double> bufferA(cols), bufferB(cols);
            cv::Mat from(rows, cols, CV_8S, cv::Scalar(0));

            // Raw pointers that are swapped after every row. Indexing the vectors directly inside
            // the loop is much slower: the compiler must assume a write through an schar* could
            // change the vectors.
            double* aboveCost{ bufferA.data() };
            double* currentCost{ bufferB.data() };

            const auto* firstRow{ energy.ptr<double>(0) };  // first row: nothing above, so the cost is the energy
            std::copy(firstRow, firstRow + cols, aboveCost);

            for (int row{ 1 }; row < rows; ++row) {
                const double* energyRow{ energy.ptr<double>(row) };
                schar* fromRow{ from.ptr<schar>(row) };

                for (int col{}; col < cols;
                     ++col) {  // same choice and tie rule as FindSeamDPFullTable
                    int best{ col };
                    if (col > 0 && aboveCost[col - 1] < aboveCost[best]) best = col - 1;
                    if (col < cols - 1 && aboveCost[col + 1] < aboveCost[best]) best = col + 1;

                    currentCost[col] = energyRow[col] + aboveCost[best];
                    fromRow[col] = static_cast<schar>(best - col);
                }
                std::swap(aboveCost, currentCost);  // the row just computed is now the "row above"
            }

            // After the last swap, aboveCost holds the costs of the last row.
            int col{ static_cast<int>(std::min_element(aboveCost, aboveCost + cols) - aboveCost) };

            Seam seam(rows);
            for (int row{ rows - 1 }; row >= 0; --row) {
                seam[row] = col;
                col += from.at<schar>(row, col);
            }
            return seam;
        }
    }


    Seam FindSeamDP(const cv::Mat& energy, bool useFullTable) {
        if (energy.empty() || energy.type() != CV_64F)
            throw std::invalid_argument("FindSeamDP: energy must be a non-empty CV_64F matrix");

        if (useFullTable) return FindSeamDPFullTable(energy);
        else return FindSeamDPTwoRows(energy);
    }
}  // namespace seamcarve
