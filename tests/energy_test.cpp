// Unit tests for the energy functions.
//
// Every method is checked for basic properties (size, type, flat image, edge image) and for
// invariances that must hold for any sensible energy (brightness offset, mirror, transpose).
// The Sobel methods are also compared pixel by pixel with an independent reference written with
// plain loops, and against a value worked out by hand.
//
// A method that still throws "not implemented" is reported as SKIP, so only code that is
// implemented but wrong makes the test fail. If every method is skipped the whole test is
// reported as skipped to CTest (exit code 77).
//
// Note: OpenCV objects are built with parentheses here on purpose; with braces, OpenCV's
// initializer-list constructors can be picked instead of the one you meant.
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

#include "seamcarve/energy.hpp"

namespace {
    constexpr int CTEST_SKIP_CODE{ 77 };
    constexpr int IMAGE_SIZE{ 16 };
    constexpr double TOLERANCE{ 1e-9 };

    int failureCount{};

    using EnergyFunction = std::function<cv::Mat(const cv::Mat&)>;

    /**
    * @brief Records a failed check and prints which method and property failed.
    * @param[in] condition	- The property that should hold.
    * @param[in] method	- Name of the energy method under test.
    * @param[in] what	- Description of the property.
    */
    void Check(bool condition, const std::string& method, const std::string& what) {
        if (condition) return;
        ++failureCount;
        std::cerr << "FAIL [" << method << "] " << what << "\n";
    }

    /**
    * @brief Checks that an energy map is a CV_64F matrix of the expected size.
    * @param[in] energy	- The matrix returned by the method.
    * @param[in] rows	- Expected number of rows.
    * @param[in] cols	- Expected number of columns.
    * @param[in] method	- Name used in the message.
    * @return true if the shape and type are right (so later checks can safely read the values).
    */
    bool HasExpectedShape(const cv::Mat& energy, int rows, int cols, const std::string& method) {
        const bool ok{ energy.rows == rows && energy.cols == cols && energy.type() == CV_64FC1 };
        Check(ok, method, "output must be a single-channel CV_64F matrix with the input's size");
        return ok;
    }

    /**
    * @brief Creates an image with the same colour everywhere.
    * @return A IMAGE_SIZE x IMAGE_SIZE CV_8UC3 image.
    */
    cv::Mat MakeFlatImage() {
        return cv::Mat(IMAGE_SIZE, IMAGE_SIZE, CV_8UC3, cv::Scalar(90, 120, 200));
    }

    /**
    * @brief Creates an image that is black on the left half and white on the right half.
    * @return A IMAGE_SIZE x IMAGE_SIZE CV_8UC3 image with a vertical edge between the two halves.
    */
    cv::Mat MakeVerticalEdgeImage() {
        cv::Mat image(IMAGE_SIZE, IMAGE_SIZE, CV_8UC3, cv::Scalar(0, 0, 0));
        image(cv::Rect(IMAGE_SIZE / 2, 0, IMAGE_SIZE - IMAGE_SIZE / 2, IMAGE_SIZE)).setTo(cv::Scalar(255, 255, 255));
        return image;
    }

    /**
    * @brief Creates a random grayscale image (reproducible: the same seed gives the same image).
    * @param[in] rows	- Number of rows.
    * @param[in] cols	- Number of columns.
    * @param[in] maxValue	- Pixel values are in [0, maxValue).
    * @param[in] seed	- Random seed.
    * @return A CV_8UC1 image.
    */
    cv::Mat MakeRandomGray(int rows, int cols, int maxValue, int seed) {
        cv::Mat gray(rows, cols, CV_8UC1);
        cv::RNG rng(seed);
        rng.fill(gray, cv::RNG::UNIFORM, 0, maxValue);
        return gray;
    }

    /**
    * @brief Turns a grayscale image into a BGR image whose three channels are equal, so the
    * grayscale value of every pixel is known exactly whatever conversion the method uses.
    * @param[in] gray	- CV_8UC1 image.
    * @return A CV_8UC3 image.
    */
    cv::Mat GrayToBgr(const cv::Mat& gray) {
        const std::vector<cv::Mat> channels{ gray, gray, gray };
        cv::Mat bgr;
        cv::merge(channels, bgr);
        return bgr;
    }

    /**
    * @brief Reflect-101 border rule (gfedcb|abcdefgh|gfedcba): the pixel beyond the edge is the
    * mirror of the pixel one step inside, without repeating the edge pixel.
    * @param[in] index	- Row or column, possibly one step outside the image.
    * @param[in] size	- Number of rows or columns.
    * @return The index to read instead.
    */
    int ReflectIndex(int index, int size) {
        if (size == 1) return 0;
        if (index < 0) return -index;
        if (index >= size) return 2 * size - 2 - index;
        return index;
    }

    /**
    * @brief Independent reference for the expected Sobel energy, written with plain loops.
    * Convention: energy = |dx| + |dy| with the 3x3 Sobel kernels and the reflect-101 border.
    * If you decide on a different convention for ComputeEnergySobel, change it here too.
    * @param[in] gray	- CV_8UC1 image.
    * @return CV_64F matrix with the expected energy.
    */
    cv::Mat ReferenceSobelEnergy(const cv::Mat& gray) {
        constexpr int kernelX[3][3]{ { -1, 0, 1 }, { -2, 0, 2 }, { -1, 0, 1 } };
        constexpr int kernelY[3][3]{ { -1, -2, -1 }, { 0, 0, 0 }, { 1, 2, 1 } };

        cv::Mat energy(gray.rows, gray.cols, CV_64F);
        for (int row{}; row < gray.rows; ++row) {
            for (int col{}; col < gray.cols; ++col) {
                int dx{}, dy{};
                for (int kernelRow{ -1 }; kernelRow <= 1; ++kernelRow) {
                    for (int kernelCol{ -1 }; kernelCol <= 1; ++kernelCol) {
                        const int value{ gray.at<uchar>(ReflectIndex(row + kernelRow, gray.rows), ReflectIndex(col + kernelCol, gray.cols)) };
                        dx += kernelX[kernelRow + 1][kernelCol + 1] * value;
                        dy += kernelY[kernelRow + 1][kernelCol + 1] * value;
                    }
                }
                energy.at<double>(row, col) = std::abs(dx) + std::abs(dy);
            }
        }
        return energy;
    }

    /**
    * @brief Checks the basic properties of one energy method (size, type, flat and edge images).
    * @param[in] name	- Name used in the messages.
    * @param[in] computeEnergy	- The method under test.
    * @throws std::logic_error "not implemented" if the method is still a stub.
    */
    void TestMethod(const std::string& name, const EnergyFunction& computeEnergy) {
        // A flat image has no change anywhere, so the energy must be zero everywhere (borders too).
        const cv::Mat flatEnergy(computeEnergy(MakeFlatImage()));
        if (!HasExpectedShape(flatEnergy, IMAGE_SIZE, IMAGE_SIZE, name)) return;

        double flatMin{}, flatMax{};
        cv::minMaxLoc(flatEnergy, &flatMin, &flatMax);
        Check(flatMax <= TOLERANCE, name, "a flat image must have zero energy");

        // A vertical edge: energy next to the edge, none far away from it, and never negative.
        const cv::Mat edgeEnergy(computeEnergy(MakeVerticalEdgeImage()));
        if (!HasExpectedShape(edgeEnergy, IMAGE_SIZE, IMAGE_SIZE, name)) return;

        double edgeMin{}, edgeMax{};
        cv::minMaxLoc(edgeEnergy, &edgeMin, &edgeMax);
        Check(edgeMin >= 0.0, name, "energy must not be negative");

        const int row{ IMAGE_SIZE / 2 };
        const double nextToEdge{ edgeEnergy.at<double>(row, IMAGE_SIZE / 2 - 1) + edgeEnergy.at<double>(row, IMAGE_SIZE / 2) };
        Check(nextToEdge > 0.0, name, "energy must be positive next to a vertical edge");
        Check(edgeEnergy.at<double>(row, IMAGE_SIZE / 4) <= TOLERANCE, name, "energy must be zero in a flat area away from the edge");
    }

    /**
    * @brief Checks things that must hold for any gradient-based energy, without knowing the exact
    * values: adding a constant to every pixel changes nothing, and mirroring or transposing the
    * image mirrors or transposes the energy (the energy depends only on differences, and
    * |dx| + |dy| treats both directions the same). A method that forgets dy, for example, fails the
    * transpose check.
    * @param[in] name	- Name used in the messages.
    * @param[in] computeEnergy	- The method under test.
    * @throws std::logic_error "not implemented" if the method is still a stub.
    */
    void TestInvariances(const std::string& name, const EnergyFunction& computeEnergy) {
        constexpr int rows{ 13 }, cols{ 21 };  // not square, odd sizes
        const cv::Mat gray(MakeRandomGray(rows, cols, 200, 7));
        const cv::Mat energy(computeEnergy(GrayToBgr(gray)));
        if (!HasExpectedShape(energy, rows, cols, name)) return;

        cv::Mat brighter;
        gray.convertTo(brighter, CV_8U, 1.0, 50.0);  // values stay below 250, so nothing saturates
        const cv::Mat brighterEnergy(computeEnergy(GrayToBgr(brighter)));
        if (HasExpectedShape(brighterEnergy, rows, cols, name)) {
            Check(cv::norm(energy, brighterEnergy, cv::NORM_INF) <= TOLERANCE, name,
                  "adding a constant to every pixel must not change the energy");
        }

        cv::Mat mirrored, expectedMirrored;
        cv::flip(gray, mirrored, 1);
        cv::flip(energy, expectedMirrored, 1);
        const cv::Mat mirroredEnergy(computeEnergy(GrayToBgr(mirrored)));
        if (HasExpectedShape(mirroredEnergy, rows, cols, name)) {
            Check(cv::norm(mirroredEnergy, expectedMirrored, cv::NORM_INF) <= TOLERANCE, name,
                  "the energy of a mirrored image must be the mirrored energy");
        }

        cv::Mat transposed, expectedTransposed;
        cv::transpose(gray, transposed);
        cv::transpose(energy, expectedTransposed);
        const cv::Mat transposedEnergy(computeEnergy(GrayToBgr(transposed)));
        if (HasExpectedShape(transposedEnergy, cols, rows, name)) {
            Check(cv::norm(transposedEnergy, expectedTransposed, cv::NORM_INF) <= TOLERANCE, name,
                  "the energy of a transposed image must be the transposed energy (are dx and dy both used?)");
        }
    }

    /**
    * @brief Checks that a Sobel energy gives the right numbers: one value worked out by hand, then
    * every pixel of several images (including non-square and tiny ones) against ReferenceSobelEnergy.
    * @param[in] name	- Name used in the messages.
    * @param[in] computeEnergy	- The method under test.
    * @throws std::logic_error "not implemented" if the method is still a stub.
    */
    void TestSobelValues(const std::string& name, const EnergyFunction& computeEnergy) {
        // Worked by hand: I(row, col) = 10 * col + 20 * row. Away from the border, a 3x3 Sobel gives
        //   dx = (2 * 10) * (1 + 2 + 1) = 80 and dy = (2 * 20) * (1 + 2 + 1) = 160,
        // so |dx| + |dy| = 240 for every interior pixel.
        cv::Mat ramp(5, 5, CV_8UC1);
        for (int row{}; row < ramp.rows; ++row)
            for (int col{}; col < ramp.cols; ++col)
                ramp.at<uchar>(row, col) = static_cast<uchar>(10 * col + 20 * row);

        const cv::Mat rampEnergy(computeEnergy(GrayToBgr(ramp)));
        if (!HasExpectedShape(rampEnergy, 5, 5, name)) return;

        bool rampOk{ true };
        for (int row{ 1 }; row <= 3; ++row)
            for (int col{ 1 }; col <= 3; ++col)
                if (std::abs(rampEnergy.at<double>(row, col) - 240.0) > TOLERANCE) rampOk = false;
        Check(rampOk, name, "on the ramp image every interior pixel must be |dx| + |dy| = 80 + 160 = 240 (got "
                            + std::to_string(rampEnergy.at<double>(2, 2)) + " at row 2, col 2)");

        // Every pixel, including the borders, against the independent reference.
        const std::vector<std::pair<int, int>> shapes{ { 16, 16 }, { 13, 21 }, { 21, 13 }, { 3, 3 }, { 2, 5 } };
        int seed{ 100 };
        for (const std::pair<int, int>& shape : shapes) {
            const cv::Mat gray(MakeRandomGray(shape.first, shape.second, 256, seed++));
            const cv::Mat expected(ReferenceSobelEnergy(gray));
            const cv::Mat actual(computeEnergy(GrayToBgr(gray)));
            const std::string size{ std::to_string(shape.first) + "x" + std::to_string(shape.second) };
            if (!HasExpectedShape(actual, shape.first, shape.second, name + " (" + size + ")")) continue;

            cv::Mat difference;
            cv::absdiff(actual, expected, difference);
            double largest{};
            cv::Point where;
            cv::minMaxLoc(difference, nullptr, &largest, nullptr, &where);
            Check(largest <= TOLERANCE, name,
                  "on a random " + size + " image the energy differs from the reference at row " + std::to_string(where.y)
                  + ", col " + std::to_string(where.x) + ": expected " + std::to_string(expected.at<double>(where))
                  + ", got " + std::to_string(actual.at<double>(where)));
        }
    }

    struct Method {
        std::string name;
        EnergyFunction compute;
        bool isSobel;  // also compare the exact values with the Sobel reference
    };
}  // namespace

/**
* @brief Runs every energy method through the checks.
* @return 0 if all implemented methods pass, 1 if one fails, 77 (CTest "skipped") if none is implemented yet.
*/
int main() {
    const std::vector<Method> methods{
        { "gradient", [](const cv::Mat& image) { return seamcarve::ComputeEnergyGradient(image); }, false },
        { "sobel", [](const cv::Mat& image) { return seamcarve::ComputeEnergySobel(image); }, true },
        { "divide-and-conquer", [](const cv::Mat& image) { return seamcarve::ComputeEnergyDivideAndConquer(image, 8); }, false },
        { "default (ComputeEnergy)", [](const cv::Mat& image) { return seamcarve::ComputeEnergy(image); }, true },
    };

    int skippedCount{};
    for (const Method& method : methods) {
        const int failuresBefore{ failureCount };
        try {
            TestMethod(method.name, method.compute);
            TestInvariances(method.name, method.compute);
            if (method.isSobel) TestSobelValues(method.name, method.compute);
            std::cout << (failureCount == failuresBefore ? "PASS  " : "FAIL  ") << method.name << "\n";
        } catch (const std::logic_error& e) {
            if (std::string(e.what()).find("not implemented") == std::string::npos) {
                ++failureCount;
                std::cerr << "FAIL [" << method.name << "] unexpected error: " << e.what() << "\n";
                continue;
            }
            ++skippedCount;
            std::cout << "SKIP  " << method.name << " (not implemented yet)\n";
        } catch (const std::exception& e) {
            ++failureCount;
            std::cerr << "FAIL [" << method.name << "] unexpected error: " << e.what() << "\n";
        }
    }

    if (failureCount > 0) return 1;
    if (skippedCount == static_cast<int>(methods.size())) return CTEST_SKIP_CODE;
    return 0;
}
