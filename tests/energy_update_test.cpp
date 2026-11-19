// Tests for the incremental energy update (ComputeEnergyAt, UpdateEnergyAfterSeamRemoval) and for Carve(),
// which uses it. The update must give exactly the map a full recompute gives, after every single seam,
// and Carve() must give exactly the image the plain "recompute everything after every seam" loop gives.
// Images are random noise (the worst case for the update: every pixel differs from its neighbours).
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "seamcarve/energy.hpp"
#include "seamcarve/seam_carver.hpp"
#include "seamcarve/seam_finders.hpp"

namespace {
    int failureCount{};

    /**
    * @brief Records a failed check and prints what failed.
    * @param[in] condition	- The property that should hold.
    * @param[in] what	- Description of the property.
    */
    void Check(bool condition, const std::string& what) {
        if (condition) return;
        ++failureCount;
        std::cerr << "FAIL " << what << "\n";
    }

    /**
    * @brief Creates a reproducible random BGR image.
    * @param[in] rows	- Number of rows.
    * @param[in] cols	- Number of columns.
    * @param[in] seed	- Random seed.
    * @return A CV_8UC3 image.
    */
    cv::Mat RandomImage(int rows, int cols, int seed) {
        cv::Mat image(rows, cols, CV_8UC3);
        cv::RNG rng(seed);
        rng.fill(image, cv::RNG::UNIFORM, 0, 256);
        return image;
    }

    struct Setup {
        std::string name;
        seamcarve::EnergyMethod method;
        seamcarve::DivideAndConquer type;
        int baseSize;
    };

    /**
    * @brief The reference: the plain loop that recomputes the whole energy map after every seam.
    * @param[in] image	- Input image.
    * @param[in] targetWidth	- Width to stop at.
    * @param[in] setup	- Energy method (and base size).
    * @return The carved image.
    */
    cv::Mat SlowCarveWidth(const cv::Mat& image, int targetWidth, const Setup& setup) {
        cv::Mat current{ image.clone() };
        while (current.cols > targetWidth) {
            const cv::Mat energy(seamcarve::ComputeEnergy(current, setup.method, setup.type, setup.baseSize));
            current = seamcarve::RemoveSeam(current, seamcarve::FindSeamDP(energy), seamcarve::Orientation::VERTICAL);
        }
        return current;
    }

    /**
    * @brief The reference version of Carve(): width first, then the height on the transposed image.
    * @param[in] image	- Input image.
    * @param[in] targetWidth	- Target width.
    * @param[in] targetHeight	- Target height.
    * @param[in] setup	- Energy method (and base size).
    * @return The carved image.
    */
    cv::Mat SlowCarve(const cv::Mat& image, int targetWidth, int targetHeight, const Setup& setup) {
        cv::Mat result(SlowCarveWidth(image, targetWidth, setup));
        cv::Mat transposed;
        cv::transpose(result, transposed);
        transposed = SlowCarveWidth(transposed, targetHeight, setup);
        cv::transpose(transposed, result);
        return result;
    }
}  // namespace

/**
* @brief Runs the update and Carve() tests.
* @return 0 if all pass, 1 otherwise.
*/
int main() {
    using seamcarve::EnergyMethod;
    using seamcarve::DivideAndConquer;
    const std::vector<Setup> setups{
        { "gradient", EnergyMethod::GRADIENT, DivideAndConquer::RECURSION, 32 },
        { "sobel", EnergyMethod::SOBEL, DivideAndConquer::RECURSION, 32 },
        { "divide and conquer, recursion, base 3", EnergyMethod::DIVIDE_AND_CONQUER, DivideAndConquer::RECURSION, 3 },
        { "divide and conquer, recursion, base 32", EnergyMethod::DIVIDE_AND_CONQUER, DivideAndConquer::RECURSION, 32 },
        { "divide and conquer, iteration, base 1", EnergyMethod::DIVIDE_AND_CONQUER, DivideAndConquer::ITERATION, 1 },
        { "divide and conquer, iteration, base 32", EnergyMethod::DIVIDE_AND_CONQUER, DivideAndConquer::ITERATION, 32 },
    };

    // 1. After every seam the updated map equals a full recompute (sizes include 1-3 pixel dimensions: the borders).
    for (const Setup& setup : setups) {
        int checked{}, wrong{};
        for (int rows : { 1, 2, 3, 5, 17, 40 }) {
            for (int cols : { 2, 3, 4, 9, 30 }) {
                cv::Mat image(RandomImage(rows, cols, rows * 100 + cols));
                cv::Mat energy(seamcarve::ComputeEnergy(image, setup.method, setup.type, setup.baseSize));

                while (image.cols > 1) {
                    const seamcarve::Seam seam{ seamcarve::FindSeamDP(energy) };
                    image = seamcarve::RemoveSeam(image, seam, seamcarve::Orientation::VERTICAL);
                    energy = seamcarve::RemoveSeam(energy, seam, seamcarve::Orientation::VERTICAL);

                    seamcarve::UpdateEnergyAfterSeamRemoval(energy, image, seam, setup.method, setup.type, setup.baseSize);

                    ++checked;
                    const cv::Mat full(seamcarve::ComputeEnergy(image, setup.method, setup.type, setup.baseSize));
                    if (cv::norm(energy, full, cv::NORM_INF) != 0) ++wrong;
                }
            }
        }
        Check(wrong == 0, setup.name + ": " + std::to_string(wrong) + " of " + std::to_string(checked) + " updated maps differ from a full recompute");
        std::cout << (wrong == 0 ? "PASS  " : "FAIL  ") << "update is exact after every seam: " << setup.name << " (" << checked << " seams)\n";
    }

    // 2. Carve() gives exactly the image the slow loop gives, in both directions.
    for (const Setup& setup : setups) {
        int wrong{}, total{};
        for (const std::pair<int, int>& target : std::vector<std::pair<int, int>>{ { 60, 50 }, { 45, 33 }, { 20, 50 }, { 60, 5 }, { 2, 2 }, { 60, 50 } }) {
            const cv::Mat image(RandomImage(50, 60, target.first * 7 + target.second));
            seamcarve::CarveOptions options;
            options.energyMethod = setup.method;
            options.divideAndConquerType = setup.type;
            options.baseSize = setup.baseSize;
            const cv::Mat fast(seamcarve::Carve(image, target.first, target.second, options));
            const cv::Mat slow(SlowCarve(image, target.first, target.second, setup));
            ++total;
            if (fast.size() != slow.size() || cv::norm(fast, slow, cv::NORM_INF) != 0) ++wrong;
        }
        Check(wrong == 0, setup.name + ": Carve() differs from the slow loop in " + std::to_string(wrong) + " cases");
        std::cout << (wrong == 0 ? "PASS  " : "FAIL  ") << "Carve() equals the slow loop: " << setup.name << " (" << total << " sizes)\n";
    }

    // 3. Wrong sizes are rejected.
    {
        cv::Mat energy(4, 4, CV_64F, cv::Scalar(1)), image(4, 3, CV_8UC3, cv::Scalar(0));
        bool threw{};
        try {
            seamcarve::UpdateEnergyAfterSeamRemoval(energy, image, seamcarve::Seam(4, 1), EnergyMethod::SOBEL);
        } catch (const std::invalid_argument&) { threw = true; }
        Check(threw, "mismatched energy / image sizes must throw std::invalid_argument");

        threw = false;
        image = cv::Mat(4, 4, CV_8UC3, cv::Scalar(0));
        try {
            seamcarve::UpdateEnergyAfterSeamRemoval(energy, image, seamcarve::Seam(3, 1), EnergyMethod::SOBEL);
        } catch (const std::invalid_argument&) { threw = true; }
        Check(threw, "a seam of the wrong length must throw std::invalid_argument");
        std::cout << (threw ? "PASS  " : "FAIL  ") << "bad sizes rejected\n";
    }

    return failureCount > 0 ? 1 : 0;
}
