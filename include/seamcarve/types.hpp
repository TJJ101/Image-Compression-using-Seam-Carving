#pragma once
#include <string>
#include <vector>

namespace seamcarve {
    enum class Algorithm { 
        DYNAMIC_PROGRAMMING, 
        GREEDY, 
        GRAPH_CUT 
    };

    enum class Orientation { 
        VERTICAL, HORIZONTAL 
    };

    // A seam is stored as one index per row (vertical seam, size == rows: the column
    // removed in each row) or one index per column (horizontal seam, size == cols:
    // the row removed in each column).
    using Seam = std::vector<int>;

    /**
    * @brief Converts an algorithm name from the command line into an Algorithm value.
    * Accepted names are "dp", "greedy" and "graphcut".
    * @param[in] name	- Algorithm name.
    * @return The matching Algorithm value.
    * @throws std::invalid_argument If the name is not recognised.
    */
    Algorithm ParseAlgorithm(const std::string& name);

    /**
    * @brief Converts an Algorithm value into its command-line name.
    * @param[in] algo	- Algorithm value.
    * @return "dp", "greedy" or "graphcut".
    */
    std::string ToString(Algorithm algo);
}  // namespace seamcarve
