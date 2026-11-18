#include "seamcarve/types.hpp"
#include <stdexcept>

namespace seamcarve {
    Algorithm ParseAlgorithm(const std::string& name) {
        if (name == "dp") return Algorithm::DYNAMIC_PROGRAMMING;
        if (name == "greedy") return Algorithm::GREEDY;
        if (name == "graphcut") return Algorithm::GRAPH_CUT;
        throw std::invalid_argument("unknown algorithm '" + name + "' (expected dp, greedy or graphcut)");
    }

    std::string ToString(Algorithm algo) {
        switch (algo) {
            case Algorithm::DYNAMIC_PROGRAMMING: return "dp";
            case Algorithm::GREEDY: return "greedy";
            case Algorithm::GRAPH_CUT: return "graphcut";
        }
        return "unknown";
    }

    EnergyMethod ParseEnergyMethod(const std::string& name) {
        if (name == "gradient") return EnergyMethod::GRADIENT;
        if (name == "sobel") return EnergyMethod::SOBEL;
        if (name == "dc") return EnergyMethod::DIVIDE_AND_CONQUER;
        throw std::invalid_argument("unknown energy method '" + name + "' (expected gradient, sobel or dc)");
    }

    std::string ToString(EnergyMethod method) {
        switch (method) {
            case EnergyMethod::GRADIENT: return "gradient";
            case EnergyMethod::SOBEL: return "sobel";
            case EnergyMethod::DIVIDE_AND_CONQUER: return "dc";
        }
        return "unknown";
    }

}  // namespace seamcarve
