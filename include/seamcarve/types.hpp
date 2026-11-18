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

    /// Ways to compute the energy map (Task 1a). See energy.hpp.
    enum class EnergyMethod {
        GRADIENT,            // finite differences between neighbouring pixels
        SOBEL,               // Sobel filter (convolution)
        DIVIDE_AND_CONQUER   // split the image into blocks, compute each, merge (Question 2b)
    };

    enum class DivideAndConquer {
        RECURSION,
        ITERATION
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

    /**
    * @brief Converts an energy method name from the command line into an EnergyMethod value.
    * Accepted names are "gradient", "sobel" and "dc" (divide and conquer).
    * @param[in] name	- Energy method name.
    * @return The matching EnergyMethod value.
    * @throws std::invalid_argument If the name is not recognised.
    */
    EnergyMethod ParseEnergyMethod(const std::string& name);

    /**
    * @brief Converts an EnergyMethod value into its command-line name.
    * @param[in] method	- Energy method value.
    * @return "gradient", "sobel" or "dc".
    */
    std::string ToString(EnergyMethod method);

    /**
    * @brief Converts a divide-and-conquer type name from the command line into a DivideAndConquer value.
    * Accepted names are "recursion" and "iteration".
    * @param[in] name	- Divide-and-conquer type name.
    * @return The matching DivideAndConquer value.
    * @throws std::invalid_argument If the name is not recognised.
    */
    DivideAndConquer ParseDivideAndConquerType(const std::string& name);

    /**
    * @brief Converts a DivideAndConquer value into its command-line name.
    * @param[in] type	- Divide-and-conquer type value.
    * @return "recursion" or "iteration".
    */
    std::string ToString(DivideAndConquer type);
}  // namespace seamcarve
