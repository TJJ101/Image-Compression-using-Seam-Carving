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
        // TODO(Question 2b): divide and conquer.
        //  1. If the block is at most baseSize in both directions, compute it directly (Sobel or gradient).
        //  2. Otherwise split it in two, recurse on each half and merge into one result.
        //  3. A pixel next to a cut needs its neighbour from the other half: give every block a 1-pixel
        //     overlap (a "halo") so the merged result equals the unsplit computation.
        //  4. Compare running time against the direct methods; the halves are independent, so this is also
        //     where multithreading (analysis 5d) could be tried.

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
}  // namespace seamcarve
