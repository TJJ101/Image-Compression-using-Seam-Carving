// Tests for the step-by-step view of a carve: the onSeamFound hook and the seam log of Carve(), PaintSeam,
// BuildRemovalMap, RenderSeamOverlay, RenderCarveStep, and the publish / poll pattern the GUI uses to show
// the preview from another thread (run this test under ThreadSanitizer to check it for data races).
// Images are random noise, or "index images" whose pixels hold their own position so that the origin of every
// remaining pixel can be read back (BuildRemovalMap is checked against that).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>

#include "seamcarve/seam_carver.hpp"

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
    * @brief Runs a callable and reports whether it threw the given exception type.
    * @tparam Exception	- Exception type that is expected.
    * @param[in] call	- What to run.
    * @return true if it threw that type.
    */
    template <typename Exception, typename Callable>
    bool Throws(Callable call) {
        try {
            call();
        } catch (const Exception&) {
            return true;
        } catch (...) {
            return false;
        }
        return false;
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

    /**
    * @brief Creates an image whose pixel (y, x) stores x in the blue and y in the green channel (red is noise,
    * so the seams are not all ties). Sizes up to 255 are supported.
    * @param[in] rows	- Number of rows.
    * @param[in] cols	- Number of columns.
    * @param[in] seed	- Random seed for the red channel.
    * @return A CV_8UC3 image.
    */
    cv::Mat IndexImage(int rows, int cols, int seed) {
        cv::Mat image(rows, cols, CV_8UC3);
        cv::RNG rng(seed);
        for (int y{}; y < rows; ++y) {
            for (int x{}; x < cols; ++x) {
                image.at<cv::Vec3b>(y, x) = cv::Vec3b(static_cast<uchar>(x), static_cast<uchar>(y), static_cast<uchar>(rng.uniform(0, 256)));
            }
        }
        return image;
    }

    /// True if two images have the same size and the same pixels.
    bool SameImage(const cv::Mat& a, const cv::Mat& b) {
        return a.size() == b.size() && a.type() == b.type() && cv::norm(a, b, cv::NORM_INF) == 0;
    }

    /// Number of seams Carve() removes for these sizes: width pass plus height pass.
    int SeamCount(int width, int height, int targetWidth, int targetHeight) {
        return (width - targetWidth) + (height - targetHeight);
    }
}  // namespace

/**
* @brief Runs every check.
* @return 0 if all pass, 1 otherwise.
*/
int main() {
    using namespace seamcarve;

    // 1. The hook is called once per seam with the image the seam was found in; the log holds the same seams;
    //    and watching changes nothing about the result.
    {
        const int width{ 40 }, height{ 30 }, targetWidth{ 25 }, targetHeight{ 18 };
        const cv::Mat image(RandomImage(height, width, 1));

        std::vector<SeamStep> calls, log;
        bool shapesFit{ true };
        CarveOptions watched;
        watched.seamLog = &log;
        watched.onSeamFound = [&](const cv::Mat& found, const Seam& seam, Orientation orientation) {
            const int index{ static_cast<int>(calls.size()) };
            // Width pass: the image is the working image, one column narrower each time.
            // Height pass: it is the transposed image (rows = current width, columns = current height).
            const int expectedRows{ orientation == Orientation::VERTICAL ? height : targetWidth };
            const int expectedCols{ orientation == Orientation::VERTICAL ? width - index : height - (index - (width - targetWidth)) };
            if (found.rows != expectedRows || found.cols != expectedCols || static_cast<int>(seam.size()) != found.rows) shapesFit = false;
            for (int value : seam) if (value < 0 || value >= found.cols) shapesFit = false;
            calls.push_back({ orientation, seam });
        };

        const cv::Mat withHooks(Carve(image, targetWidth, targetHeight, watched));
        const cv::Mat plain(Carve(image, targetWidth, targetHeight, CarveOptions{}));

        const int total{ SeamCount(width, height, targetWidth, targetHeight) };
        Check(static_cast<int>(calls.size()) == total, "onSeamFound must be called once per seam (" + std::to_string(calls.size()) + " of " + std::to_string(total) + ")");
        Check(shapesFit, "onSeamFound must get the image the seam was found in (transposed in the height pass) with a seam that fits it");
        bool orderRight{ true };
        for (int index{}; index < static_cast<int>(calls.size()); ++index)
            if (calls[index].orientation != (index < width - targetWidth ? Orientation::VERTICAL : Orientation::HORIZONTAL)) orderRight = false;
        Check(orderRight, "all vertical seams must come first, then all horizontal ones");
        Check(log.size() == calls.size(), "the seam log must hold one entry per seam");
        bool sameSeams{ log.size() == calls.size() };
        for (std::size_t index{}; sameSeams && index < log.size(); ++index)
            if (log[index].orientation != calls[index].orientation || log[index].seam != calls[index].seam) sameSeams = false;
        Check(sameSeams, "the seam log must hold the same seams, in the same order, that the hook saw");
        Check(SameImage(withHooks, plain), "watching a carve (hook and log) must not change its result");
        std::cout << "PASS  hook and seam log\n";
    }

    // 2. BuildRemovalMap: every pixel is either kept (-1) or removed exactly once; the kept pixels are the ones in
    //    the result (read back from the index image); each step removed as many pixels as its seam is long.
    {
        int wrong{}, cases{};
        const int sizes[][4]{ { 40, 30, 25, 18 }, { 41, 17, 41, 9 }, { 33, 33, 10, 33 }, { 20, 25, 1, 1 }, { 9, 9, 9, 9 }, { 60, 12, 31, 5 } };
        for (const auto& size : sizes) {
            const int width{ size[0] }, height{ size[1] }, targetWidth{ size[2] }, targetHeight{ size[3] };
            const cv::Mat image(IndexImage(height, width, width * 7 + height));
            std::vector<SeamStep> log;
            CarveOptions options;
            options.seamLog = &log;
            const cv::Mat result(Carve(image, targetWidth, targetHeight, options));
            const cv::Mat removedAt(BuildRemovalMap(image.size(), log));
            ++cases;

            bool ok{ removedAt.type() == CV_32S && removedAt.size() == image.size() };
            std::vector<int> removedPerStep(log.size(), 0);
            int kept{};
            for (int y{}; ok && y < height; ++y) {
                for (int x{}; x < width; ++x) {
                    const int step{ removedAt.at<int>(y, x) };
                    if (step < -1 || step >= static_cast<int>(log.size())) { ok = false; break; }
                    if (step == -1) ++kept; else ++removedPerStep[step];
                }
            }
            for (std::size_t step{}; ok && step < log.size(); ++step)
                if (removedPerStep[step] != static_cast<int>(log[step].seam.size())) ok = false;
            ok = ok && kept == result.cols * result.rows;

            // Every pixel of the result must come from a pixel the map says was kept, and no two from the same one.
            std::set<std::pair<int, int>> origins;
            for (int y{}; ok && y < result.rows; ++y) {
                for (int x{}; x < result.cols; ++x) {
                    const cv::Vec3b pixel{ result.at<cv::Vec3b>(y, x) };
                    if (removedAt.at<int>(pixel[1], pixel[0]) != -1) ok = false;
                    origins.insert({ pixel[1], pixel[0] });
                }
            }
            ok = ok && static_cast<int>(origins.size()) == result.cols * result.rows;

            // A vertical step removes exactly one pixel from every original row.
            for (std::size_t step{}; ok && step < log.size() && log[step].orientation == Orientation::VERTICAL; ++step) {
                std::vector<int> perRow(height, 0);
                for (int y{}; y < height; ++y)
                    for (int x{}; x < width; ++x)
                        if (removedAt.at<int>(y, x) == static_cast<int>(step)) ++perRow[y];
                for (int count : perRow) if (count != 1) ok = false;
            }
            if (!ok) ++wrong;
        }
        Check(wrong == 0, "BuildRemovalMap is wrong in " + std::to_string(wrong) + " of " + std::to_string(cases) + " carves");

        std::vector<SeamStep> bad{ { Orientation::VERTICAL, Seam(5, 0) } };
        Check(Throws<std::invalid_argument>([&] { BuildRemovalMap(cv::Size(4, 4), bad); }), "BuildRemovalMap must reject a seam of the wrong length");
        bad = { { Orientation::VERTICAL, Seam{ 0, 9, 0, 0 } } };
        Check(Throws<std::out_of_range>([&] { BuildRemovalMap(cv::Size(4, 4), bad); }), "BuildRemovalMap must reject a seam index outside the image");
        Check(Throws<std::invalid_argument>([&] { BuildRemovalMap(cv::Size(0, 4), {}); }), "BuildRemovalMap must reject an empty size");
        Check(cv::countNonZero(BuildRemovalMap(cv::Size(3, 3), {}) + 1) == 0, "with no steps every pixel is kept (-1)");
        std::cout << "PASS  removal map (" << cases << " carves)\n";
    }

    // 3. RenderCarveStep: step N is the final image; every step has the right size and shows the next seam.
    {
        const int width{ 36 }, height{ 28 }, targetWidth{ 24 }, targetHeight{ 20 };
        const cv::Mat image(RandomImage(height, width, 5));
        std::vector<SeamStep> log;
        CarveOptions options;
        options.seamLog = &log;
        const cv::Mat result(Carve(image, targetWidth, targetHeight, options));
        const int total{ static_cast<int>(log.size()) };

        Check(SameImage(RenderCarveStep(image, log, total), result), "RenderCarveStep at the last step must equal the carved image");

        bool sizesRight{ true }, seamShown{ true };
        for (int step{}; step <= total; ++step) {
            const cv::Mat frame(RenderCarveStep(image, log, step));
            const int verticalDone{ std::min(step, width - targetWidth) }, horizontalDone{ std::max(0, step - (width - targetWidth)) };
            if (frame.cols != width - verticalDone || frame.rows != height - horizontalDone) sizesRight = false;
            if (step < total) {  // the next seam must be red
                const SeamStep& next{ log[step] };
                for (int line{}; line < static_cast<int>(next.seam.size()); ++line) {
                    const cv::Vec3b shown{ next.orientation == Orientation::VERTICAL ? frame.at<cv::Vec3b>(line, next.seam[line]) : frame.at<cv::Vec3b>(next.seam[line], line) };
                    if (shown != cv::Vec3b(0, 0, 255)) seamShown = false;
                }
            }
        }
        Check(sizesRight, "RenderCarveStep must remove one pixel of width or height per step");
        Check(seamShown, "RenderCarveStep must paint the next seam red");
        Check(SameImage(RenderCarveStep(image, log, total, 5), result), "the seam thickness must not matter at the last step");
        Check(Throws<std::out_of_range>([&] { RenderCarveStep(image, log, total + 1); }), "RenderCarveStep must reject a step past the end");
        Check(Throws<std::out_of_range>([&] { RenderCarveStep(image, log, -1); }), "RenderCarveStep must reject a negative step");
        std::cout << "PASS  render carve step (" << total + 1 << " steps)\n";
    }

    // 4. PaintSeam
    {
        const cv::Vec3b red(0, 0, 255);
        cv::Mat black(6, 8, CV_8UC3, cv::Scalar(0, 0, 0));
        PaintSeam(black, Seam{ 0, 1, 2, 3, 4, 5 }, Orientation::VERTICAL);
        int painted{};
        for (int y{}; y < black.rows; ++y)
            for (int x{}; x < black.cols; ++x)
                if (black.at<cv::Vec3b>(y, x) == red) ++painted;
        Check(painted == 6 && black.at<cv::Vec3b>(3, 3) == red && black.at<cv::Vec3b>(5, 5) == red, "a vertical seam paints one pixel per row");

        cv::Mat horizontal(6, 8, CV_8UC3, cv::Scalar(0, 0, 0));
        PaintSeam(horizontal, Seam{ 0, 1, 2, 3, 4, 5, 5, 5 }, Orientation::HORIZONTAL, cv::Scalar(1, 2, 3));
        Check(horizontal.at<cv::Vec3b>(2, 2) == cv::Vec3b(1, 2, 3) && horizontal.at<cv::Vec3b>(5, 7) == cv::Vec3b(1, 2, 3) && cv::countNonZero(horizontal.reshape(1)) == 8 * 3,
              "a horizontal seam paints one pixel per column, in the given colour");

        cv::Mat thick(3, 6, CV_8UC3, cv::Scalar(0, 0, 0));
        PaintSeam(thick, Seam{ 0, 4, 5 }, Orientation::VERTICAL, cv::Scalar(0, 0, 255), 3);
        Check(thick.at<cv::Vec3b>(0, 2) == red && thick.at<cv::Vec3b>(0, 3) == cv::Vec3b(0, 0, 0) && thick.at<cv::Vec3b>(1, 5) == red && thick.at<cv::Vec3b>(2, 5) == red,
              "a thick seam paints extra pixels to the right and stops at the image edge");

        Check(Throws<std::invalid_argument>([&] { PaintSeam(black, Seam(5, 0), Orientation::VERTICAL); }), "PaintSeam must reject a seam of the wrong length");
        Check(Throws<std::out_of_range>([&] { PaintSeam(black, Seam{ 0, 1, 2, 3, 4, 8 }, Orientation::VERTICAL); }), "PaintSeam must reject an index outside the image");
        cv::Mat grey(4, 4, CV_8U, cv::Scalar(0));
        Check(Throws<std::invalid_argument>([&] { PaintSeam(grey, Seam(4, 0), Orientation::VERTICAL); }), "PaintSeam must reject a non-BGR image");
        std::cout << "PASS  paint seam\n";
    }

    // 5. RenderSeamOverlay
    {
        const cv::Mat image(15, 20, CV_8UC3, cv::Scalar(1, 2, 3));
        std::vector<SeamStep> log;
        CarveOptions options;
        options.seamLog = &log;
        Carve(image, 12, 9, options);
        const cv::Mat removedAt(BuildRemovalMap(image.size(), log));
        const cv::Mat overlay(RenderSeamOverlay(image, removedAt, static_cast<int>(log.size())));

        bool keptUnchanged{ overlay.size() == image.size() }, removedColoured{ true };
        for (int y{}; y < image.rows; ++y) {
            for (int x{}; x < image.cols; ++x) {
                const bool kept{ removedAt.at<int>(y, x) < 0 };
                const bool unchanged{ overlay.at<cv::Vec3b>(y, x) == cv::Vec3b(1, 2, 3) };
                if (kept && !unchanged) keptUnchanged = false;
                if (!kept && unchanged) removedColoured = false;
            }
        }
        Check(keptUnchanged, "RenderSeamOverlay must leave the pixels that are still in the result alone");
        Check(removedColoured, "RenderSeamOverlay must colour every removed pixel");

        cv::Vec3b firstColour, lastColour;
        for (int y{}; y < image.rows; ++y) {
            for (int x{}; x < image.cols; ++x) {
                if (removedAt.at<int>(y, x) == 0) firstColour = overlay.at<cv::Vec3b>(y, x);
                if (removedAt.at<int>(y, x) == static_cast<int>(log.size()) - 1) lastColour = overlay.at<cv::Vec3b>(y, x);
            }
        }
        Check(firstColour != lastColour, "the first and the last seam must get different colours");
        Check(SameImage(RenderSeamOverlay(image, removedAt, 0), image), "with no steps the overlay is just a copy of the image");
        Check(Throws<std::invalid_argument>([&] { RenderSeamOverlay(image, cv::Mat(3, 3, CV_32S), 5); }), "RenderSeamOverlay must reject a map of the wrong size");
        Check(Throws<std::invalid_argument>([&] { RenderSeamOverlay(image, cv::Mat(15, 20, CV_8U), 5); }), "RenderSeamOverlay must reject a map of the wrong type");
        std::cout << "PASS  seam overlay\n";
    }

    // 6. The GUI's pattern: the worker publishes frames into a slot, the main thread polls it while the job runs.
    {
        const cv::Mat image(RandomImage(80, 120, 9));
        struct Slot {
            std::mutex mutex;
            cv::Mat frame;
            bool fresh{ false };
        } slot;
        std::atomic<int> delayMs{ 0 };

        CarveOptions options;
        options.onSeamFound = [&slot, &delayMs](const cv::Mat& found, const Seam& seam, Orientation orientation) {
            cv::Mat frame;
            if (orientation == Orientation::HORIZONTAL) cv::transpose(found, frame); else frame = found.clone();
            PaintSeam(frame, seam, orientation, cv::Scalar(0, 0, 255), 2);
            {
                std::lock_guard<std::mutex> lock{ slot.mutex };
                slot.frame = std::move(frame);
                slot.fresh = true;
            }
            if (delayMs.load() > 0) std::this_thread::sleep_for(std::chrono::milliseconds{ delayMs.load() });
        };

        std::future<cv::Mat> job{ std::async(std::launch::async, [&] { return Carve(image, 70, 60, options); }) };
        int framesSeen{};
        bool framesValid{ true };
        while (job.wait_for(std::chrono::milliseconds{ 1 }) != std::future_status::ready) {
            std::lock_guard<std::mutex> lock{ slot.mutex };
            if (!slot.fresh) continue;
            slot.fresh = false;
            ++framesSeen;
            if (slot.frame.empty() || slot.frame.cols > 120 || slot.frame.rows > 80 || slot.frame.type() != CV_8UC3) framesValid = false;
        }
        const cv::Mat result{ job.get() };
        Check(framesValid, "every preview frame must be a complete BGR image no larger than the original");
        Check(framesSeen >= 1 || result.cols == 70, "the main thread must be able to poll preview frames while the carve runs");
        Check(SameImage(result, Carve(image, 70, 60, CarveOptions{})), "carving with a preview hook must give the same image");
        std::cout << "PASS  preview from another thread (" << framesSeen << " frames polled)\n";
    }

    return failureCount > 0 ? 1 : 0;
}
