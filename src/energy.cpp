#include "seamcarve/energy.hpp"
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <algorithm>
#include <stack>

namespace seamcarve {
    cv::Mat ComputeEnergy(const cv::Mat& image, EnergyMethod method, DivideAndConquer type, int baseSize) {
        if (image.empty() || image.type() != CV_8UC3)
            throw std::invalid_argument("Image is empty or isn't 8-bit, 3 channel BGR");

        switch (method) {
            case EnergyMethod::GRADIENT:           return ComputeEnergyGradient(image);
            case EnergyMethod::SOBEL:              return ComputeEnergySobel(image);
            case EnergyMethod::DIVIDE_AND_CONQUER: return ComputeEnergyDivideAndConquer(image, type, baseSize);
        }
        throw std::invalid_argument("ComputeEnergy: unknown energy method");
    }

    cv::Mat ComputeEnergyGradient(const cv::Mat& image) {
        cv::Mat grayImg;
        cv::cvtColor(image, grayImg, cv::COLOR_BGR2GRAY);  // convert image to grayscale

        cv::Mat energy(grayImg.rows, grayImg.cols, CV_64F);

        for (int row{}; row < grayImg.rows; ++row) {
            const auto* upRow{ grayImg.ptr<uchar>(std::max(row - 1, 0)) };      
            const auto* downRow{ grayImg.ptr<uchar>(std::min(row + 1, grayImg.rows - 1)) };
            const auto* currRow{ grayImg.ptr<uchar>(row) };

            auto* energyRow{ energy.ptr<double>(row) };

            for (int col{}; col < grayImg.cols; ++col) {
                const int left{ std::max(col - 1, 0) }, right{ std::min(col + 1, grayImg.cols - 1) };
                const int dx{ currRow[right] - currRow[left] }, dy{ downRow[col] - upRow[col] };
                energyRow[col] = std::abs(dx) + std::abs(dy);
            }
        }
        return energy;
    }

    cv::Mat ComputeEnergySobel(const cv::Mat& image) {
        cv::Mat grayImg;
        cv::cvtColor(image, grayImg, cv::COLOR_BGR2GRAY);  // convert image to grayscale

        cv::Mat dx, dy;
        cv::Sobel(grayImg, dx, CV_64F, 1, 0);
        cv::Sobel(grayImg, dy, CV_64F, 0, 1);
       
        return cv::abs(dx) + cv::abs(dy);   // calculate energy using |dx| + |dy|
    }

    cv::Mat ComputeEnergyDivideAndConquer(const cv::Mat& image, DivideAndConquer type, int baseSize) {
        switch (type) {
            case DivideAndConquer::RECURSION: return ComputeEnergyDivideAndConquerWithRecursion(image, baseSize);
            case DivideAndConquer::ITERATION: return ComputeEnergyDivideAndConquerWithIteration(image, baseSize);
        }
        throw std::invalid_argument("ComputeEnergyDivideAndConquer: Invalid Divide and Conquer type.");
    }

    cv::Mat ComputeEnergyDivideAndConquerWithRecursion(const cv::Mat& image, int baseSize) {
        if (baseSize < 3) 
            throw std::invalid_argument("ComputeEnergyDivideAndConquerWithRecursion: baseSize must be at least 3.");
           
        if (image.rows <= baseSize && image.cols <= baseSize) 
            return ComputeEnergySobel(image);   // base case

        cv::Mat energy(image.rows, image.cols, CV_64F);

        if (image.cols >= image.rows) {
            const int mid{ image.cols / 2 };
            const cv::Mat left(ComputeEnergyDivideAndConquerWithRecursion(image.colRange(0, mid + 1), baseSize));
            const cv::Mat right(ComputeEnergyDivideAndConquerWithRecursion(image.colRange(mid - 1, image.cols), baseSize));

            left.colRange(0, mid).copyTo(energy.colRange(0, mid));
            right.colRange(1, right.cols).copyTo(energy.colRange(mid, image.cols));
        } else {
            const int mid{ image.rows / 2 };
            const cv::Mat top(ComputeEnergyDivideAndConquerWithRecursion(image.rowRange(0, mid + 1), baseSize));
            const cv::Mat bottom(ComputeEnergyDivideAndConquerWithRecursion(image.rowRange(mid - 1, image.rows), baseSize));

            top.rowRange(0, mid).copyTo(energy.rowRange(0, mid));
            bottom.rowRange(1, bottom.rows).copyTo(energy.rowRange(mid, image.rows));
        }

        return energy;
    }

    namespace {
        struct Region {
            int rowStart{}, rowEnd{}, colStart{}, colEnd{};
        };

        // Rows per chunk when the energy is updated after a seam is removed (see UpdateEnergyAfterSeamRemoval).
        // Smaller chunks give narrower crops but more calls and more margin; larger ones give fewer calls but wider crops.
        // 32 to 64 was fastest in the benchmarks (Sobel: 256 rows 1.0 ms, 64 rows 0.7 ms, 8 rows 1.7 ms per seam).
        constexpr int UPDATE_CHUNK_ROWS{ 64 };
    }

    cv::Mat ComputeEnergyDivideAndConquerWithIteration(const cv::Mat& image, int baseSize) {
        if (baseSize < 1) throw std::invalid_argument("ComputeEnergyDivideAndConquerWithIteration: baseSize must be at least 1");

        cv::Mat energy(image.rows, image.cols, CV_64F);
        std::stack<Region> stack;
        stack.push({ 0, image.rows, 0, image.cols });

        while (!stack.empty()) {
            const Region region{ stack.top() };
            stack.pop();
            const int height{ region.rowEnd - region.rowStart }, width{ region.colEnd - region.colStart };

            if (height <= baseSize && width <= baseSize) {  // base case
                const int rowBegin{ std::max(region.rowStart - 1, 0) }, rowFinish{ std::min(region.rowEnd + 1, image.rows) };
                const int colBegin{ std::max(region.colStart - 1, 0) }, colFinish{ std::min(region.colEnd + 1, image.cols) };

                const cv::Mat block(ComputeEnergySobel(
                    image(cv::Range(rowBegin, rowFinish), cv::Range(colBegin, colFinish)))
                );

                block(cv::Range(region.rowStart - rowBegin, region.rowEnd - rowBegin),cv::Range(region.colStart - colBegin, region.colEnd - colBegin)).
                    copyTo(energy(cv::Range(region.rowStart, region.rowEnd), 
                        cv::Range(region.colStart, region.colEnd)));
            } else if (width >= height){
                const int mid{ region.colStart + width / 2 };
                stack.push({ region.rowStart, region.rowEnd, region.colStart, mid });
                stack.push({ region.rowStart, region.rowEnd, mid, region.colEnd});
            } else {
                const int mid{ region.rowStart + height / 2 };
                stack.push({ region.rowStart, mid, region.colStart, region.colEnd });
                stack.push({ mid, region.rowEnd, region.colStart, region.colEnd });
            }
        }
        return energy;
    }

    void UpdateEnergyAfterSeamRemoval(cv::Mat& energy, const cv::Mat& image, const Seam& seam, EnergyMethod method, 
        DivideAndConquer type, int baseSize) {
        if (energy.size() != image.size() || static_cast<int>(seam.size()) != energy.rows)
            throw std::invalid_argument("UpdateEnergyAfterSeamRemoval: energy, image, and seam sizes do not match");

        // The rows are handled in chunks. One box around the whole seam is wide, because a seam wanders sideways
        // as it goes down; a chunk only has to cover the part of the seam that lies in it, which is narrow.
        // So instead of one big crop there are a few small ones, each hugging the seam (about 2% of the image
        // in total instead of about 16%).
        for (int chunkFirst{}; chunkFirst < energy.rows; chunkFirst += UPDATE_CHUNK_ROWS) {
            const int chunkLast{ std::min(chunkFirst + UPDATE_CHUNK_ROWS, energy.rows) };  // one past the chunk's last row

            // A pixel's energy depends on its 3x3 neighbourhood, so what matters is the seam in the chunk's rows
            // and in the row just beyond each side of the chunk (hence the -1 / +1).
            const auto [lowest, highest]{
                std::minmax_element(seam.begin() + std::max(chunkFirst - 1, 0), seam.begin() + std::min(chunkLast + 1, energy.rows))
            };

            // Columns whose energy changed: from the pixel left of the leftmost seam pixel up to the rightmost one
            // (the seam's two neighbours moved next to each other). A safe superset: extra pixels recompute to the same value.
            const int changedFirst{ std::max(*lowest - 1, 0) },
                changedLast{ std::min(*highest, energy.cols - 1) };

            // The crop is one pixel larger on every side so the filter has real neighbours for the changed pixels.
            // Where the crop touches the image border its edge is the image's edge, so the filter treats it exactly as
            // it would in the full image.
            const int cropFirstRow{ std::max(chunkFirst - 1, 0) }, cropLastRow{ std::min(chunkLast + 1, energy.rows) };
            const int cropFirstCol{ std::max(changedFirst - 1, 0) },cropLastCol{ std::min(changedLast + 1, energy.cols - 1) };
            const cv::Mat crop(image, cv::Range(cropFirstRow, cropLastRow),
                               cv::Range(cropFirstCol, cropLastCol + 1));  // a view, no copy

            // Run the chosen method on the crop, then copy back only the chunk's changed pixels. The crop's extra
            // edge pixels are not copied: they lacked a neighbour on the outside, so their values would be wrong.
            const cv::Mat fresh(ComputeEnergy(crop, method, type, baseSize));
            fresh(cv::Range(chunkFirst - cropFirstRow, chunkLast - cropFirstRow),
                          cv::Range(changedFirst - cropFirstCol, changedLast - cropFirstCol + 1))
                .copyTo(energy(cv::Range(chunkFirst, chunkLast), cv::Range(changedFirst, changedLast + 1))
            );
        }
    }

}  // namespace seamcarve
