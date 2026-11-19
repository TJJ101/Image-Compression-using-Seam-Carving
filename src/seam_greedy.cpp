#include <stdexcept>
#include <random>
#include <algorithm>
#include <optional>
#include "seamcarve/seam_finders.hpp"

namespace seamcarve {
    Seam FindSeamGreedy(const cv::Mat& energy, unsigned seed) {
        std::optional<std::mt19937> rng;

        Seam s{};
        s.reserve(energy.rows);
        // Finding for cheapest in first row
        const double* row{ energy.ptr<double>(0) };
        double cheapest{ energy.at<double>(0, 0) };
        int   cheapestIndex{ 0 };
        for (int i{}; i < energy.cols; i++){
            if (row[i] <= cheapest) {
                cheapest = row[i];
                cheapestIndex = i;
            }
        }

        s.push_back(cheapestIndex);

        for (int i{ 1 }; i < energy.rows; ++i){
            // Find the cheapest of the 3 neighbours below with bounds checking
            int   li{ std::max(s[i - 1] - 1, 0) };
            int   ti{ s[i - 1] };
            int   ri{ std::min(s[i - 1] + 1, energy.cols - 1) };

            double left{ energy.at<double>(i, li) };
            double top{ energy.at<double>(i, ti) };
            double right{ energy.at<double>(i, ri) };

            if (top <= left && top <= right) s.push_back(ti);
            else if (left < right)           s.push_back(li);
            else if (right < left)           s.push_back(ri);
            else {
                if(!rng) rng.emplace(seed != 0 ? seed : std::random_device{}());    // build rng device only when needed
                s.push_back(((*rng)() & 1) ? li : ri);    // Tiebreaking rand for left and right
            }
        }

        return s;
    }
}  // namespace seamcarve
