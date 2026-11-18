#pragma once
#include <optional>
#include <string>
#include <vector>
#include "seamcarve/types.hpp"

namespace seamcarve {
    struct CliOptions {
        std::string input;
        std::string output;  // empty -> derived from input + algorithm
        Algorithm algorithm{ Algorithm::DYNAMIC_PROGRAMMING };
        EnergyMethod energyMethod{ EnergyMethod::SOBEL };
        std::optional<int> width;   // target width in pixels (default: unchanged)
        std::optional<int> height;  // target height in pixels (default: unchanged)
        bool verbose{ false };
        bool help{ false };
        bool gui{ false };  // open the interactive window instead of running the CLI
    };

    /**
    * @brief Parses the command-line arguments into a CliOptions struct.
    * Returns early with help set if -h/--help is present. With no arguments at all (e.g. when the
    * program is double-clicked) or with -g/--gui, gui is set and only an optional --input is used.
    * @param[in] args	- Arguments, excluding the program name.
    * @return The parsed options.
    * @throws std::invalid_argument With a user-readable message on bad arguments.
    */
    CliOptions ParseArgs(const std::vector<std::string>& args);

    /**
    * @brief Builds the help text describing every command-line option.
    * @return The usage message.
    */
    std::string Usage();

    /**
    * @brief Derives the default output path when --output is not given.
    * For example, "photo.png" with Algorithm::GREEDY gives "photo_greedy.png".
    * @param[in] input	- Input image path.
    * @param[in] algo	- Algorithm used, appended to the file name.
    * @return The output path.
    */
    std::string DefaultOutputPath(const std::string& input, Algorithm algo);

}  // namespace seamcarve
