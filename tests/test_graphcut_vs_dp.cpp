// Compares FindSeamGraphCut against FindSeamDP. Both are optimal, so on every energy map their
// seams must cost the same (the seams themselves may differ when several seams tie).
// Exits with 77 (reported as skipped by CTest) while FindSeamDP is not implemented.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "seamcarve/seam_finders.hpp"
#include "seamcarve/seam_graphcut.hpp"

using seamcarve::FindSeamDP;
using seamcarve::FindSeamGraphCut;
using seamcarve::SeamCost;

int failures{ 0 };

void Check(const std::string& name, bool ok) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    if (!ok) failures++;
}

bool Near(double a, double b) {
    return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::fabs(a));
}

// Random energy map. With wholeNumbers the values are 0..7, so many seams tie.
cv::Mat RandomEnergy(int rows, int cols, unsigned seed, bool wholeNumbers) {
    std::mt19937 rng{ seed };
    std::uniform_real_distribution<double> dist{ 0.0, 255.0 };
    cv::Mat energy(rows, cols, CV_64F);
    for (int row{ 0 }; row < rows; ++row)
        for (int col{ 0 }; col < cols; ++col) {
            const double value{ dist(rng) };
            energy.at<double>(row, col) = wholeNumbers ? std::floor(value / 32) : value;
        }
    return energy;
}

bool DpImplemented() {
    try {
        FindSeamDP(cv::Mat(1, 1, CV_64F, cv::Scalar(0)));
    } catch (const std::logic_error& e) {
        if (std::string(e.what()).find("not implemented") != std::string::npos) return false;
    }
    return true;
}

// Both seams must be valid (SeamCost throws otherwise) and cost the same.
bool SameCost(const cv::Mat& energy) {
    try {
        const double dpCost{ SeamCost(energy, FindSeamDP(energy)) };
        const double graphCutCost{ SeamCost(energy, FindSeamGraphCut(energy)) };
        if (Near(dpCost, graphCutCost)) return true;
        std::cout << "  DP " << dpCost << ", graph cut " << graphCutCost << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  " << e.what() << std::endl;
    }
    return false;
}

void TestRandomMaps() {
    const std::vector<std::vector<int>> sizes{ { 10, 10 }, { 20, 30 }, { 30, 20 }, { 40, 40 }, { 1, 50 }, { 50, 1 } };
    for (const bool wholeNumbers : { false, true }) {
        for (const std::vector<int>& size : sizes) {
            bool ok{ true };
            for (unsigned seed{ 1 }; seed <= 3; ++seed)
                ok = SameCost(RandomEnergy(size[0], size[1], seed, wholeNumbers)) && ok;
            Check(std::to_string(size[0]) + "x" + std::to_string(size[1]) + (wholeNumbers ? " whole-number" : "")
                      + " random maps: same seam cost",
                  ok);
        }
    }
}

void TestSpecialMaps() {
    Check("uniform map: same seam cost", SameCost(cv::Mat(8, 12, CV_64F, cv::Scalar(3))));
    Check("all-zero map: same seam cost", SameCost(cv::Mat(8, 12, CV_64F, cv::Scalar(0))));

    // Cheapest seam along the right edge, where the graph needs its edges to the sink
    cv::Mat rightEdge(10, 10, CV_64F, cv::Scalar(50));
    rightEdge.col(9).setTo(1);
    Check("cheap last column: same seam cost", SameCost(rightEdge));

    // Cheap zigzag path that moves one column per row
    cv::Mat zigzag(12, 6, CV_64F, cv::Scalar(50));
    for (int row{ 0 }; row < zigzag.rows; ++row) zigzag.at<double>(row, row % 4 < 2 ? row % 4 : 4 - row % 4) = 1;
    Check("zigzag path: same seam cost", SameCost(zigzag));
}

int main() {
    if (!DpImplemented()) {
        std::cout << "[SKIP] FindSeamDP is not implemented yet" << std::endl;
        return 77;
    }
    TestRandomMaps();
    TestSpecialMaps();
    std::cout << "Total failures: " << failures << std::endl;
    return failures == 0 ? 0 : 1;
}
