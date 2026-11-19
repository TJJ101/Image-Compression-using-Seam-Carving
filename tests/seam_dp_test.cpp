// Unit tests for the dynamic programming seam finder FindSeamDP, in its three ways of being called:
// FindSeamDP(energy, true) (full table), FindSeamDP(energy, false) (two rows) and the default
// FindSeamDP(energy). Every finder is checked against a brute-force search over all seams, for
// validity, against greedy, with bad input, and all of them must return exactly the same seam.
//
// A finder that still throws "not implemented" is reported as SKIP (exit code 77 if all are skipped).
// OpenCV objects are built with parentheses on purpose (braces can pick initializer-list constructors).
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "seamcarve/seam_finders.hpp"

namespace {
    constexpr int CTEST_SKIP_CODE{ 77 };
    constexpr double TOLERANCE{ 1e-9 };

    int failureCount{};

    using SeamFinder = std::function<seamcarve::Seam(const cv::Mat&)>;

    struct Finder {
        std::string name;
        SeamFinder find;
    };

    /**
    * @brief Records a failed check and prints which finder and property failed.
    * @param[in] condition	- The property that should hold.
    * @param[in] finder	- Name of the finder under test.
    * @param[in] what	- Description of the property.
    */
    void Check(bool condition, const std::string& finder, const std::string& what) {
        if (condition) return;
        ++failureCount;
        std::cerr << "FAIL [" << finder << "] " << what << "\n";
    }

    /**
    * @brief Adds up the energy along a seam.
    * @param[in] energy	- Energy map (CV_64F).
    * @param[in] seam	- One column per row.
    * @return The total energy of the seam.
    */
    double SeamCost(const cv::Mat& energy, const seamcarve::Seam& seam) {
        double total{};
        for (int row{}; row < energy.rows; ++row) 
            total += energy.at<double>(row, seam[row]);
        return total;
    }

    /**
    * @brief Checks the size, the column range and the connectivity of a seam.
    * @param[in] energy	- The energy map the seam was found in.
    * @param[in] seam	- The seam to check.
    * @return true if it is a valid vertical seam.
    */
    bool IsValidSeam(const cv::Mat& energy, const seamcarve::Seam& seam) {
        if (static_cast<int>(seam.size()) != energy.rows) return false;
        for (int row{}; row < energy.rows; ++row) {
            if (seam[row] < 0 || seam[row] >= energy.cols) return false;
            if (row > 0 && std::abs(seam[row] - seam[row - 1]) > 1) return false;
        }
        return true;
    }

    /**
    * @brief Tries every possible seam starting at one pixel (only for small matrices).
    * @param[in] energy	- Energy map.
    * @param[in] row	- Current row.
    * @param[in] col	- Current column.
    * @param[in] total	- Energy collected so far.
    * @param[in,out] best	- Smallest total found so far.
    */
    void BruteForce(const cv::Mat& energy, int row, int col, double total, double& best) {
        total += energy.at<double>(row, col);
        if (row == energy.rows - 1) {
            best = std::min(best, total);
            return;
        }

        for (int next{ col - 1 }; next <= col + 1; ++next) {
            if (next >= 0 && next < energy.cols) 
                BruteForce(energy, row + 1, next, total, best);
        }
    }

    /**
    * @brief The cost of the cheapest seam, found by trying all of them.
    * @param[in] energy	- Energy map (small!).
    * @return The minimum total energy.
    */
    double BruteForceMinimum(const cv::Mat& energy) {
        double best{ std::numeric_limits<double>::infinity() };
        for (int col{}; col < energy.cols; ++col) 
            BruteForce(energy, 0, col, 0.0, best);
        return best;
    }

    /**
    * @brief Creates a reproducible random energy map.
    * @param[in] rows	- Number of rows.
    * @param[in] cols	- Number of columns.
    * @param[in] distinctValues	- If above 0, values are integers in [0, distinctValues) (lots of ties); otherwise reals in [0, 255).
    * @param[in] seed	- Random seed.
    * @return A CV_64F matrix.
    */
    cv::Mat RandomEnergy(int rows, int cols, int distinctValues, int seed) {
        cv::Mat energy(rows, cols, CV_64F);
        cv::RNG rng(seed);
        for (int row{}; row < rows; ++row) {
            for (int col{}; col < cols; ++col) {
                energy.at<double>(row, col) = distinctValues > 0 ? 
                    static_cast<double>(rng.uniform(0, distinctValues)) : rng.uniform(0.0, 255.0);
            }
        }
        return energy;
    }

    /**
    * @brief Checks one finder: the worked example, brute force on small matrices, greedy, bad input and views.
    * @param[in] name	- Name used in the messages.
    * @param[in] find	- The finder under test.
    * @throws std::logic_error "not implemented" if the finder is still a stub.
    */
    void TestFinder(const std::string& name, const SeamFinder& find) {
        // The 4x4 example from the explanation: the cheapest seam is columns 1 2 1 2 with energy 3 + 1 + 2 + 1 = 7.
        cv::Mat example(4, 4, CV_64F);
        const double values[4][4]{ { 5, 3, 4, 6 }, { 2, 8, 1, 5 }, { 7, 2, 6, 3 }, { 4, 5, 1, 9 } };
        for (int row{}; row < 4; ++row) {
            for (int col{}; col < 4; ++col) 
                example.at<double>(row, col) = values[row][col];
        }
           
        const seamcarve::Seam exampleSeam{ find(example) };
        Check(exampleSeam == seamcarve::Seam({ 1, 2, 1, 2 }), name, "the 4x4 worked example must give the seam 1 2 1 2");

        // Valid and optimal on many small matrices, including ones full of ties and constant ones.
        const std::vector<std::pair<int, int>> shapes{ 
            { 1, 1 }, { 1, 6 }, { 6, 1 }, { 2, 2 }, { 3, 3 }, { 2, 7 }, { 7, 2 }, { 5, 5 }, { 7, 7 }, { 8, 4 } 
        };

        int invalidSeams{}, notOptimal{};
        for (const std::pair<int, int>& shape : shapes) {
            for (int distinct : { 0, 3, 2, 1 }) {
                for (int trial{}; trial < 25; ++trial) {
                    const cv::Mat energy(RandomEnergy(shape.first, shape.second, distinct, trial * 13 + 1));
                    const seamcarve::Seam seam{ find(energy) };

                    if (!IsValidSeam(energy, seam)) { 
                        ++invalidSeams; 
                        continue; 
                    }

                    if (std::abs(SeamCost(energy, seam) - BruteForceMinimum(energy)) > TOLERANCE) 
                        ++notOptimal;
                }
            }
        }
            
        Check(invalidSeams == 0, name, std::to_string(invalidSeams) + " small matrices gave an invalid seam (wrong size, out of range or not connected)");
        Check(notOptimal == 0, name, std::to_string(notOptimal) + " small matrices gave a seam that is not the cheapest (checked against brute force)");

        // The optimal seam can never cost more than greedy's.
        int worseThanGreedy{};
        for (int trial{}; trial < 40; ++trial) {
            const cv::Mat energy(RandomEnergy(40 + trial, 30 + trial % 20, trial % 3 == 0 ? 4 : 0, trial + 500));
            if (SeamCost(energy, find(energy)) > SeamCost(energy, seamcarve::FindSeamGreedy(energy, 1)) + TOLERANCE) 
                ++worseThanGreedy;
        }
        Check(worseThanGreedy == 0, name, std::to_string(worseThanGreedy) + " matrices gave a seam more expensive than greedy's");

        // Bad input must be rejected, not crash.
        int notRejected{};
        const std::vector<cv::Mat> badInputs{ cv::Mat(), cv::Mat(0, 4, CV_64F), cv::Mat(4, 4, CV_32F, cv::Scalar(1)), cv::Mat(4, 4, CV_8U, cv::Scalar(1)), cv::Mat(4, 4, CV_64FC3, cv::Scalar(1, 2, 3)) };
        for (const cv::Mat& bad : badInputs) {
            try {
                find(bad);
                ++notRejected;
            } catch (const std::invalid_argument&) { }
        }
        Check(notRejected == 0, name, std::to_string(notRejected) + " bad inputs (empty, wrong type) were not rejected with std::invalid_argument");

        // A sub-matrix view (rows not contiguous in memory) must behave like a copy.
        const cv::Mat big(RandomEnergy(40, 60, 0, 99));
        const cv::Mat view(big, cv::Range(5, 30), cv::Range(10, 50));
        Check(find(view) == find(view.clone()), name, "a non-continuous view must give the same seam as a copy of it");
    }
}  // namespace

/**
* @brief Runs every dynamic programming finder through the checks, then checks they agree.
* @return 0 if all pass, 1 if one fails, 77 (CTest "skipped") if none is implemented yet.
*/
int main() {
    const std::vector<Finder> finders{
        { "FindSeamDP(energy, true)  [full table]", [](const cv::Mat& energy) { return seamcarve::FindSeamDP(energy, true); } },
        { "FindSeamDP(energy, false) [two rows]", [](const cv::Mat& energy) { return seamcarve::FindSeamDP(energy, false); } },
        { "FindSeamDP(energy)        [default]", [](const cv::Mat& energy) { return seamcarve::FindSeamDP(energy); } },
    };

    int skippedCount{};
    std::vector<const Finder*> implemented;
    for (const Finder& finder : finders) {
        const int failuresBefore{ failureCount };
        try {
            TestFinder(finder.name, finder.find);
            std::cout << (failureCount == failuresBefore ? "PASS  " : "FAIL  ") << finder.name << "\n";
            implemented.push_back(&finder);
        } catch (const std::logic_error& e) {
            if (std::string(e.what()).find("not implemented") == std::string::npos) {
                ++failureCount;
                std::cerr << "FAIL [" << finder.name << "] unexpected error: " << e.what() << "\n";
                continue;
            }
            ++skippedCount;
            std::cout << "SKIP  " << finder.name << " (not implemented yet)\n";
        } catch (const std::exception& e) {
            ++failureCount;
            std::cerr << "FAIL [" << finder.name << "] unexpected error: " << e.what() << "\n";
        }
    }

    // All implemented finders break ties the same way, so they must return exactly the same seam.
    int differentSeams{}, compared{};
    for (int trial{}; trial < 300; ++trial) {
        const cv::Mat energy(RandomEnergy(5 + trial % 90, 3 + trial % 70, trial % 2 == 0 ? 3 : 0, trial + 2000));
        for (std::size_t index{ 1 }; index < implemented.size(); ++index) {
            ++compared;
            if (implemented[index]->find(energy) != implemented[0]->find(energy)) ++differentSeams;
        }
    }
    if (implemented.size() > 1) {
        Check(differentSeams == 0, "all DP finders", std::to_string(differentSeams) + " of " + std::to_string(compared) + " comparisons gave a different seam");
        std::cout << (differentSeams == 0 ? "PASS  " : "FAIL  ") << "all DP finders return the same seam (" << compared << " comparisons)\n";
    }

    if (failureCount > 0) return 1;
    if (skippedCount == static_cast<int>(finders.size())) return CTEST_SKIP_CODE;
    return 0;
}
