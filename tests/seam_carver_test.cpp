// Unit tests for the seam carver: RemoveSeam (vertical, horizontal and bad seams) and Carve (output
// size for every algorithm and energy method, no-op, input left unchanged, and rejecting bad targets).
//
// OpenCV objects are built with parentheses on purpose (braces can pick initializer-list constructors).
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "seamcarve/seam_carver.hpp"

namespace {
    int failureCount{};

    /**
    * @brief Records a failed check and prints which test and property failed.
    * @param[in] condition      - The property that should hold.
    * @param[in] test   - Name of the test.
    * @param[in] what   - Description of the property.
    */
    void Check(bool condition, const std::string& test, const std::string& what) {
        if (condition) return;
        ++failureCount;
        std::cerr << "FAIL [" << test << "] " << what << "\n";
    }

    /**
    * @brief Returns true if calling the function throws the given exception type.
    * @param[in] function       - The code to run.
    * @return true if Exception was thrown, false if nothing or something else was thrown.
    */
    template <typename Exception>
    bool Throws(const std::function<void()>& function) {
        try {
            function();
        } catch (const Exception&) {
            return true;
        } catch (...) {
            return false;
        }
        return false;
    }

    /**
    * @brief Creates a BGR image where every pixel holds its column number (or row number),
    * so it is easy to see which pixel was removed.
    * @param[in] rows   - Number of rows.
    * @param[in] cols   - Number of columns.
    * @param[in] byRow  - If true, pixels hold their row number instead of their column number.
    * @return A CV_8UC3 image.
    */
    cv::Mat IndexImage(int rows, int cols, bool byRow) {
        cv::Mat image(rows, cols, CV_8UC3);
        for (int row{}; row < rows; ++row) {
            for (int col{}; col < cols; ++col) {
                const uchar value{ static_cast<uchar>(byRow ? row : col) };
                image.at<cv::Vec3b>(row, col) = cv::Vec3b(value, value, value);
            }
        }
        return image;
    }

    /**
    * @brief Creates a reproducible random BGR image.
    * @param[in] rows   - Number of rows.
    * @param[in] cols   - Number of columns.
    * @param[in] seed   - Random seed.
    * @return A CV_8UC3 image.
    */
    cv::Mat RandomImage(int rows, int cols, int seed) {
        cv::Mat image(rows, cols, CV_8UC3);
        cv::RNG rng(seed);
        rng.fill(image, cv::RNG::UNIFORM, 0, 256);
        return image;
    }

    /**
    * @brief Removing a vertical seam drops exactly one pixel per row, at the seam's column.
    * @param[in] name   - Name used in the messages.
    */
    void TestRemoveSeamVertical(const std::string& name) {
        // 3x4 image, pixel value = column. Seam 1 2 0 removes column 1 from row 0, 2 from row 1, 0 from row 2.
        const cv::Mat image(IndexImage(3, 4, false));
        const cv::Mat result(seamcarve::RemoveSeam(image, { 1, 2, 0 }, seamcarve::Orientation::VERTICAL));

        Check(result.rows == 3 && result.cols == 3, name, "a 3x4 image must become 3x3");
        Check(result.type() == image.type(), name, "the image type must not change");
        if (result.rows != 3 || result.cols != 3) return;

        const int expected[3][3]{ { 0, 2, 3 }, { 0, 1, 3 }, { 1, 2, 3 } };
        bool correct{ true };
        for (int row{}; row < 3; ++row) {
            for (int col{}; col < 3; ++col) {
                if (result.at<cv::Vec3b>(row, col)[0] != expected[row][col]) correct = false;
            }
        }
        Check(correct, name, "the remaining pixels must be the ones next to the seam, in order");
    }

    /**
    * @brief Removing a horizontal seam drops exactly one pixel per column, at the seam's row.
    * @param[in] name   - Name used in the messages.
    */
    void TestRemoveSeamHorizontal(const std::string& name) {
        // 4x3 image, pixel value = row. Seam 0 1 2 removes row 0 from column 0, 1 from column 1, 2 from column 2.
        const cv::Mat image(IndexImage(4, 3, true));
        const cv::Mat result(seamcarve::RemoveSeam(image, { 0, 1, 2 }, seamcarve::Orientation::HORIZONTAL));

        Check(result.rows == 3 && result.cols == 3, name, "a 4x3 image must become 3x3");
        if (result.rows != 3 || result.cols != 3) return;

        const int expectedByColumn[3][3]{ { 1, 2, 3 }, { 0, 2, 3 }, { 0, 1, 3 } };
        bool correct{ true };
        for (int col{}; col < 3; ++col) {
            for (int row{}; row < 3; ++row) {
                if (result.at<cv::Vec3b>(row, col)[0] != expectedByColumn[col][row]) correct = false;
            }
        }
        Check(correct, name, "the remaining pixels must be the ones next to the seam, in order");
    }

    /**
    * @brief Bad seams and images must be rejected with an exception, not crash.
    * @param[in] name   - Name used in the messages.
    */
    void TestRemoveSeamBadInput(const std::string& name) {
        using seamcarve::Orientation;
        using seamcarve::RemoveSeam;
        const cv::Mat image(IndexImage(3, 4, false));

        Check(Throws<std::invalid_argument>([&] { RemoveSeam(image, { 0, 0 }, Orientation::VERTICAL); }),
              name, "a seam shorter than the image must throw std::invalid_argument");
        Check(Throws<std::invalid_argument>([&] { RemoveSeam(image, { 0, 0, 0, 0 }, Orientation::HORIZONTAL); }) == false,
              name, "a horizontal seam with one index per column must be accepted");
        Check(Throws<std::invalid_argument>([&] { RemoveSeam(image, { 0, 0, 0 }, Orientation::HORIZONTAL); }),
              name, "a horizontal seam with the wrong length must throw std::invalid_argument");
        Check(Throws<std::out_of_range>([&] { RemoveSeam(image, { 0, -1, 0 }, Orientation::VERTICAL); }),
              name, "a seam index of -1 must throw std::out_of_range");
        Check(Throws<std::out_of_range>([&] { RemoveSeam(image, { 0, 4, 0 }, Orientation::VERTICAL); }),
              name, "a seam index equal to cols must throw std::out_of_range");
        Check(Throws<std::invalid_argument>([&] { RemoveSeam(IndexImage(3, 1, false), { 0, 0, 0 }, Orientation::VERTICAL); }),
              name, "a 1-column image must throw std::invalid_argument");
        Check(Throws<std::invalid_argument>([&] { RemoveSeam(cv::Mat(), {}, Orientation::VERTICAL); }),
              name, "an empty image must throw std::invalid_argument");
    }

    /**
    * @brief Carve must reach the exact target size, keep the type and leave the input untouched.
    * @param[in] name   - Name used in the messages.
    * @param[in] options        - Algorithm and energy method to test.
    */
    void TestCarve(const std::string& name, const seamcarve::CarveOptions& options) {
        const cv::Mat image(RandomImage(20, 30, 7));  // 20 rows x 30 columns
        const cv::Mat original(image.clone());

        struct Target { int width, height; };
        const std::vector<Target> targets{ { 25, 15 }, { 10, 20 }, { 30, 12 }, { 1, 1 } };
        for (const Target& target : targets) {
            const cv::Mat result(seamcarve::Carve(image, target.width, target.height, options));
            const std::string size{ std::to_string(target.width) + "x" + std::to_string(target.height) };
            Check(result.cols == target.width && result.rows == target.height, name,
                  "carving to " + size + " gave " + std::to_string(result.cols) + "x" + std::to_string(result.rows));
            Check(result.type() == CV_8UC3, name, "carving to " + size + " must keep the CV_8UC3 type");
        }

        Check(cv::norm(image, original, cv::NORM_INF) == 0, name, "Carve must not modify the input image");

        // Same size as the input: nothing to remove, so the pixels come back unchanged.
        const cv::Mat same(seamcarve::Carve(image, image.cols, image.rows, options));
        Check(same.size() == image.size() && cv::norm(same, image, cv::NORM_INF) == 0, name,
              "carving to the original size must return the same pixels");
    }

    /**
    * @brief Targets larger than the image, smaller than 1x1 or an empty image must be rejected.
    * @param[in] name   - Name used in the messages.
    */
    void TestCarveRejects(const std::string& name) {
        const cv::Mat image(RandomImage(20, 30, 3));
        const seamcarve::CarveOptions options{};

        Check(Throws<std::invalid_argument>([&] { seamcarve::Carve(image, image.cols + 1, image.rows, options); }),
              name, "a target wider than the image must throw std::invalid_argument");
        Check(Throws<std::invalid_argument>([&] { seamcarve::Carve(image, image.cols, image.rows + 1, options); }),
              name, "a target taller than the image must throw std::invalid_argument");
        Check(Throws<std::invalid_argument>([&] { seamcarve::Carve(image, 0, image.rows, options); }),
              name, "a width of 0 must throw std::invalid_argument");
        Check(Throws<std::invalid_argument>([&] { seamcarve::Carve(image, image.cols, -5, options); }),
              name, "a negative height must throw std::invalid_argument");
        Check(Throws<std::invalid_argument>([&] { seamcarve::Carve(cv::Mat(), 1, 1, options); }),
              name, "an empty image must throw std::invalid_argument");
    }

    /**
    * @brief Runs one test, catches unexpected exceptions and prints PASS or FAIL.
    * @param[in] name   - Name of the test.
    * @param[in] test   - The test to run.
    */
    void Run(const std::string& name, const std::function<void(const std::string&)>& test) {
        const int failuresBefore{ failureCount };
        try {
            test(name);
        } catch (const std::exception& e) {
            ++failureCount;
            std::cerr << "FAIL [" << name << "] unexpected error: " << e.what() << "\n";
        }
        std::cout << (failureCount == failuresBefore ? "PASS  " : "FAIL  ") << name << "\n";
    }
}  // namespace

/**
* @brief Runs every seam carver test.
* @return 0 if all pass, 1 if any fails.
*/
int main() {
    using seamcarve::Algorithm;
    using seamcarve::CarveOptions;
    using seamcarve::EnergyMethod;

    Run("RemoveSeam vertical", TestRemoveSeamVertical);
    Run("RemoveSeam horizontal", TestRemoveSeamHorizontal);
    Run("RemoveSeam bad input", TestRemoveSeamBadInput);

    for (Algorithm algorithm : { Algorithm::DYNAMIC_PROGRAMMING, Algorithm::GREEDY, Algorithm::GRAPH_CUT }) {
        CarveOptions options{};
        options.algorithm = algorithm;
        Run("Carve with " + seamcarve::ToString(algorithm), [&](const std::string& name) { TestCarve(name, options); });
    }

    for (EnergyMethod method : { EnergyMethod::GRADIENT, EnergyMethod::SOBEL, EnergyMethod::DIVIDE_AND_CONQUER }) {
        CarveOptions options{};
        options.energyMethod = method;
        options.baseSize = 8;  // small blocks so divide and conquer really splits the 20x30 image
        Run("Carve with energy " + seamcarve::ToString(method), [&](const std::string& name) { TestCarve(name, options); });
    }

    Run("Carve rejects bad targets", TestCarveRejects);

    return failureCount > 0 ? 1 : 0;
}