// Google Benchmark suite comparing the seam-carving implementations on the test images in data/input.
//
// Every implementation is one entry in kImpls below; adding a variant (e.g. a second max-flow
// algorithm) only needs another entry. For each test image (kImages) scaled to several widths:
//   FindSeam/<impl>/<image>/<width>   one seam on the image's energy map, with a fitted big-O (N = width)
//                                     and the seam's total energy (the "seam_energy" counter)
//   Carve/<impl>/<image>/<width>      full Carve() removing 10% of the columns, at the kCarveWidths
//   ComputeEnergy/<image>/<width>     the energy map shared by all algorithms
// Images are only scaled down (never past their own width), keeping their aspect ratio. Every
// implementation runs at the same widths (up to its own limit), so the CI summary can put them side
// by side: speed relative to the fastest, and seam energy relative to DP (which is optimal).
//
// A FindSeam whose seam is not valid (wrong length, a column out of range, or a jump of more than one
// column between rows) is reported as an error, since its timing and energy would mean nothing.
//
// An implementation that still throws "not implemented" is reported as skipped (it starts producing
// numbers as soon as it is implemented); any other exception is reported as an error. While
// ComputeEnergy is not implemented, the FindSeam benchmarks use the image's grayscale intensity as
// the energy map, and say so in their label.
//
// Run: build/seamcarve_bench [--benchmark_filter=dp] [--benchmark_repetitions=3] ...

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "seamcarve/energy.hpp"
#include "seamcarve/seam_carver.hpp"
#include "seamcarve/seam_finders.hpp"
#include "seamcarve/types.hpp"

#ifndef SEAMCARVE_DATA_DIR
#define SEAMCARVE_DATA_DIR "data/input"
#endif

namespace {
    using namespace seamcarve;

    using FindFn = Seam (*)(const cv::Mat&);

    struct Impl {
        const char* name;
        Algorithm algorithm;  // used by the Carve benchmarks
        FindFn find;          // used by the FindSeam benchmarks
        int maxWidth;         // widest image for FindSeam (graph cut is much slower)
        int carveWidth;       // images are scaled to this width for Carve
    };

    // The DP and greedy finders go through lambdas because their optional parameters (useFullTable, seed) stop them
    // converting to FindFn. Greedy gets a fixed seed so its seam, and so its seam energy, is the same in every run.
    const std::vector<Impl> kImpls = {
        { "dp", Algorithm::DYNAMIC_PROGRAMMING, [](const cv::Mat& e) { return FindSeamDP(e); }, 1024, 256 },
        { "greedy", Algorithm::GREEDY, [](const cv::Mat& e) { return FindSeamGreedy(e, 1); }, 1024, 256 },
        { "graphcut", Algorithm::GRAPH_CUT, FindSeamGraphCut, 128, 64 },
    };

    // Test images in data/input
    const char* const kImages[] = { "TowerTest.jpg", "TsunamiTest.jpg", "NoiseTest.png" };

    constexpr int kMinWidth = 32;
    constexpr int kMaxEnergyWidth = 1024;
    // Widths the Carve benchmarks run at; each implementation uses the ones up to its carveWidth
    const int kCarveWidths[] = { 64, 256 };

    // ---- Probing: does an implementation run at all? ----------------------------------------
    enum class Status { Ok, Skip, Error };

    struct Probe {
        Status status;
        std::string message;
    };

    template <class F>
    Probe Try(F&& f) {
        try {
            f();
            return { Status::Ok, {} };
        } catch (const std::logic_error& e) {
            // The stubs throw std::logic_error("...: not implemented"); other logic_errors are real bugs.
            const std::string what = e.what();
            const bool stub = what.find("not implemented") != std::string::npos;
            return { stub ? Status::Skip : Status::Error, what };
        } catch (const std::exception& e) {
            return { Status::Error, e.what() };
        }
    }

    // Registers a placeholder that shows up in the report as skipped (or as an error) instead of crashing.
    void RegisterUnavailable(const std::string& name, const Probe& probe) {
        benchmark::RegisterBenchmark(name, [probe](benchmark::State& state) {
            if (probe.status == Status::Skip) {
                state.SkipWithMessage(probe.message);
            } else {
                state.SkipWithError(probe.message);
            }
        });
    }

    // ---- Test images ------------------------------------------------------------------------
    struct Sample {
        std::string name;  // file name without the extension, e.g. "TowerTest"
        cv::Mat image;     // full resolution, 8-bit BGR
    };

    cv::Mat ScaleToWidth(const cv::Mat& image, int width) {
        if (width >= image.cols) {
            return image;
        }
        const int height = std::max(1, static_cast<int>(std::lround(image.rows * static_cast<double>(width) / image.cols)));
        cv::Mat scaled;
        cv::resize(image, scaled, cv::Size(width, height), 0, 0, cv::INTER_AREA);
        return scaled;
    }

    // Loads the test images; a missing one is reported as a skipped "LoadImage/<file>" benchmark.
    std::vector<Sample> LoadSamples() {
        std::vector<Sample> samples;
        for (const char* file : kImages) {
            const std::string path = std::string(SEAMCARVE_DATA_DIR) + "/" + file;
            cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);  // grayscale files are converted to BGR
            if (image.empty()) {
                RegisterUnavailable(std::string("LoadImage/") + file, { Status::Skip, "could not read " + path });
                continue;
            }
            std::string name = file;
            name = name.substr(0, name.find_last_of('.'));
            samples.push_back({ name, image });
        }
        return samples;
    }

    // Energy map for the FindSeam benchmarks: ComputeEnergy, or the grayscale intensity while it is not available.
    bool gUseComputeEnergy = false;

    cv::Mat EnergyFor(const cv::Mat& image) {
        if (gUseComputeEnergy) {
            return ComputeEnergy(image);
        }
        cv::Mat gray;
        cv::Mat energy;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        gray.convertTo(energy, CV_64F);
        return energy;
    }

    // ---- Seam checks ------------------------------------------------------------------------
    // Returns why a vertical seam is not valid for the energy map, or an empty string if it is.
    std::string SeamProblem(const cv::Mat& energy, const Seam& seam) {
        if (static_cast<int>(seam.size()) != energy.rows) {
            return "seam has " + std::to_string(seam.size()) + " entries for " + std::to_string(energy.rows) + " rows";
        }
        for (int row = 0; row < energy.rows; ++row) {
            if (seam[row] < 0 || seam[row] >= energy.cols) {
                return "seam column " + std::to_string(seam[row]) + " out of range in row " + std::to_string(row);
            }
            if (row > 0 && std::abs(seam[row] - seam[row - 1]) > 1) {
                return "seam jumps more than one column at row " + std::to_string(row);
            }
        }
        return {};
    }

    // Total energy of the pixels on a vertical seam (lower is better; DP finds the minimum)
    double SeamEnergy(const cv::Mat& energy, const Seam& seam) {
        double total = 0.0;
        for (int row = 0; row < energy.rows; ++row) {
            total += energy.at<double>(row, seam[row]);
        }
        return total;
    }

    // ---- Benchmarks -------------------------------------------------------------------------
    void RegisterComputeEnergy(const std::vector<Sample>& samples, const Probe& probe) {
        if (probe.status != Status::Ok) {
            RegisterUnavailable("ComputeEnergy", probe);
            return;
        }
        for (const Sample& sample : samples) {
            benchmark::RegisterBenchmark("ComputeEnergy/" + sample.name, [full = sample.image](benchmark::State& state) {
                const cv::Mat image = ScaleToWidth(full, static_cast<int>(state.range(0)));
                for (auto _ : state) {
                    cv::Mat energy = ComputeEnergy(image);
                    benchmark::DoNotOptimize(energy.data);
                }
                state.SetComplexityN(image.cols);  // N = width
            })->RangeMultiplier(2)->Range(kMinWidth, std::min(kMaxEnergyWidth, sample.image.cols))
              ->Complexity()->Unit(benchmark::kMicrosecond);
        }
    }

    void RegisterFindSeam(const Impl& impl, const std::vector<Sample>& samples, const cv::Mat& probeImage) {
        const std::string base = std::string("FindSeam/") + impl.name;
        const Probe probe = Try([&] { impl.find(EnergyFor(probeImage)); });
        if (probe.status != Status::Ok) {
            RegisterUnavailable(base, probe);
            return;
        }
        for (const Sample& sample : samples) {
            benchmark::RegisterBenchmark(base + "/" + sample.name, [find = impl.find, full = sample.image](benchmark::State& state) {
                const cv::Mat energy = EnergyFor(ScaleToWidth(full, static_cast<int>(state.range(0))));
                // Check the seam once, outside the timed loop, and keep its energy to compare against DP
                const Seam checked = find(energy);
                const std::string problem = SeamProblem(energy, checked);
                if (!problem.empty()) {
                    state.SkipWithError(problem);
                    return;
                }
                state.counters["seam_energy"] = SeamEnergy(energy, checked);
                for (auto _ : state) {
                    Seam seam = find(energy);
                    benchmark::DoNotOptimize(seam);
                }
                // N = width; with the aspect ratio fixed, DP should fit O(N^2) and greedy O(N)
                state.SetComplexityN(energy.cols);
                if (!gUseComputeEnergy) {
                    state.SetLabel("grayscale energy");
                }
            })->RangeMultiplier(2)->Range(kMinWidth, std::min(impl.maxWidth, sample.image.cols))
              ->Complexity()->Unit(benchmark::kMicrosecond);
        }
    }

    void RegisterCarve(const Impl& impl, const std::vector<Sample>& samples, const cv::Mat& probeImage) {
        CarveOptions options;
        options.algorithm = impl.algorithm;

        const std::string base = std::string("Carve/") + impl.name;
        const Probe probe = Try([&] { Carve(probeImage, probeImage.cols - 1, probeImage.rows, options); });
        if (probe.status != Status::Ok) {
            RegisterUnavailable(base, probe);
            return;
        }
        for (const Sample& sample : samples) {
            int previousWidth = 0;
            for (const int width : kCarveWidths) {
                if (width > impl.carveWidth) {
                    continue;
                }
                const cv::Mat image = ScaleToWidth(sample.image, width);
                if (image.cols == previousWidth) {
                    continue;  // the image is narrower than this width, so it was not scaled again
                }
                previousWidth = image.cols;
                const int targetWidth = image.cols - image.cols / 10;
                const std::string name = base + "/" + sample.name + "/" + std::to_string(image.cols);
                benchmark::RegisterBenchmark(name, [image, targetWidth, options](benchmark::State& state) {
                    for (auto _ : state) {
                        cv::Mat carved = Carve(image, targetWidth, image.rows, options);
                        benchmark::DoNotOptimize(carved.data);
                    }
                    state.counters["seams"] = image.cols - targetWidth;
                    state.counters["pixels"] = static_cast<double>(image.total());
                })->Unit(benchmark::kMillisecond);
            }
        }
    }
}  // namespace

int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    const std::vector<Sample> samples = LoadSamples();
    if (!samples.empty()) {
        // A small copy of a test image to check which implementations run before registering them
        const cv::Mat probeImage = ScaleToWidth(samples.front().image, 16);

        const Probe energyProbe = Try([&] { ComputeEnergy(probeImage); });
        gUseComputeEnergy = energyProbe.status == Status::Ok;

        RegisterComputeEnergy(samples, energyProbe);
        for (const Impl& impl : kImpls) {
            RegisterFindSeam(impl, samples, probeImage);
            RegisterCarve(impl, samples, probeImage);
        }
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
