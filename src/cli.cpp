#include "seamcarve/cli.hpp"

#include <stdexcept>

namespace seamcarve {

    namespace {
        int ParsePositiveInt(const std::string& flag, const std::string& text) {
            std::size_t pos{};
            int value{};
            try {
                value = std::stoi(text, &pos);
            } catch (const std::exception&) {
                pos = 0;
            }

            if (pos == 0 || pos != text.size() || value <= 0)
                throw std::invalid_argument(flag + " expects a positive integer, got '" + text + "'");

            return value;
        }

    }

    std::string Usage() {
        return
            "Usage: seamcarve --input <file> --algo <dp|greedy|graphcut> "
            "[--energy <gradient|sobel|dc>] [--width <px>] [--height <px>] [--output <file>] [--verbose]\n"
            "       seamcarve [--gui] [--input <file>]   (no arguments also opens the GUI)\n"
            "\n"
            "Options:\n"
            "  -i, --input <file>    Input image (required)\n"
            "  -a, --algo <name>     dp (default) | greedy | graphcut\n"
            "  -e, --energy <name>   gradient | sobel (default) | dc (divide and conquer)\n"
            "  -w, --width <px>      Target width  (default: unchanged, must be <= original)\n"
            "  -H, --height <px>     Target height (default: unchanged, must be <= original)\n"
            "  -o, --output <file>   Output image (default: <input>_<algo>.<ext>)\n"
            "  -v, --verbose         Print progress and timing\n"
            "  -g, --gui             Open the interactive window (default when no arguments are given)\n"
            "  -h, --help            Show this message\n";
    }

    std::string DefaultOutputPath(const std::string& input, Algorithm algo) {
        const std::size_t slash{ input.find_last_of("/\\") }, dot{ input.find_last_of('.') };
        const bool hasExt{ dot != std::string::npos && (slash == std::string::npos || dot > slash) };
        const std::string stem{ hasExt ? input.substr(0, dot) : input }, ext{ hasExt ? input.substr(dot) : ".png" };

        return stem + "_" + ToString(algo) + ext;
    }

    CliOptions ParseArgs(const std::vector<std::string>& args) {
        CliOptions opts{};
        if (args.empty()) {
            opts.gui = true;
            return opts;
        }

        for (std::size_t i{}; i < args.size(); ++i) {
            const std::string& arg{ args[i] };
            auto value = [&]() -> const std::string& {
                if (i + 1 >= args.size()) throw std::invalid_argument(arg + " requires a value");
                return args[++i];
            };

            if (arg == "-h" || arg == "--help") {
                opts.help = true;
                return opts;
            } else if (arg == "-i" || arg == "--input") {
                opts.input = value();
            } else if (arg == "-o" || arg == "--output") {
                opts.output = value();
            } else if (arg == "-a" || arg == "--algo") {
                opts.algorithm = ParseAlgorithm(value());
            } else if (arg == "-e" || arg == "--energy") {
                opts.energyMethod = ParseEnergyMethod(value());
            } else if (arg == "-w" || arg == "--width") {
                opts.width = ParsePositiveInt(arg, value());
            } else if (arg == "-H" || arg == "--height") {
                opts.height = ParsePositiveInt(arg, value());
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-g" || arg == "--gui") {
                opts.gui = true;
            } else {
                throw std::invalid_argument("unknown argument '" + arg + "'");
            }
        }

        if (opts.gui) return opts;

        if (opts.input.empty()) throw std::invalid_argument("--input is required");
        if (!opts.width && !opts.height) {
            throw std::invalid_argument("specify at least one of --width / --height");
        }
        if (opts.output.empty()) opts.output = DefaultOutputPath(opts.input, opts.algorithm);
        return opts;
    }

}  // namespace seamcarve
