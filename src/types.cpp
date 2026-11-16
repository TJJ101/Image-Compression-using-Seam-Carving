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

}  // namespace seamcarve
