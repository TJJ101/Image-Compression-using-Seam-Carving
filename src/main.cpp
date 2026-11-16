#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <opencv2/imgcodecs.hpp>

#include "seamcarve/cli.hpp"
#include "seamcarve/seam_carver.hpp"
#ifdef SEAMCARVE_WITH_GUI
#include "seamcarve/gui.hpp"
#endif

#ifdef SEAMCARVE_GUI_ONLY
// GUI-only build (Windows app): there is no command-line mode. An image path given as the first
// argument (e.g. dropped onto the exe) is loaded at startup.
int main(int argc, char** argv) {
    return seamcarve::RunGui(argc > 1 ? argv[1] : "");
}
#else
int main(int argc, char** argv) {
    using namespace seamcarve;

    CliOptions opts{};
    try {
        opts = ParseArgs(std::vector<std::string>(argv + 1, argv + argc));
    } catch (const std::invalid_argument& e) {
        std::cerr << "error: " << e.what() << "\n\n" << Usage();
        return 2;
    }
    if (opts.help) {
        std::cout << Usage();
        return 0;
    }
    if (opts.gui) {
#ifdef SEAMCARVE_WITH_GUI
        return RunGui(opts.input);
#else
        std::cerr << "error: this build has no GUI (configure with -DSEAMCARVE_BUILD_GUI=ON)\n";
        return 2;
#endif
    }

    try {
        const cv::Mat image{ cv::imread(opts.input, cv::IMREAD_COLOR) };
        if (image.empty()) {
            std::cerr << "error: cannot read image '" << opts.input << "'\n";
            return 1;
        }

        const int targetW{ opts.width.value_or(image.cols) }, targetH{ opts.height.value_or(image.rows) };
        if (targetW > image.cols || targetH > image.rows) {
            std::cerr << "error: target " << targetW << "x" << targetH
                      << " is larger than the input " << image.cols << "x" << image.rows
                      << " (only shrinking is supported)\n";
            return 2;
        }

        CarveOptions carveOpts{};
        carveOpts.algorithm = opts.algorithm;
        carveOpts.verbose = opts.verbose;

        const cv::Mat result{ Carve(image, targetW, targetH, carveOpts) };
        if (!cv::imwrite(opts.output, result)) {
            std::cerr << "error: cannot write '" << opts.output << "'\n";
            return 1;
        }
        std::cout << "wrote " << opts.output << " (" << result.cols << "x" << result.rows << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
#endif  // SEAMCARVE_GUI_ONLY
