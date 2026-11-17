#include "seamcarve/max_flow.hpp"
#include <algorithm>
#include <queue>

namespace seamcarve{
    const double EPSILON {1e-9}; // Small value to compare floating point numbers

    FlowGraph::FlowGraph(int numNodes) : graph(numNodes) {}

    void FlowGraph::AddEdge(int from, int to, double capacity) {
        if(from == to) return; // No self-loops

        // Create forward and reverse edges
        FlowEdge forwardEdge{capacity, 0.0, to, static_cast<int>(graph[to].size())};
        FlowEdge reverseEdge{0.0, 0.0, from, static_cast<int>(graph[from].size())};
        graph[from].push_back(forwardEdge);
        graph[to].push_back(reverseEdge);
    }
    int FlowGraph::NumNodes() const {
        return static_cast<int>(graph.size());
    }
    int FlowGraph::AugmentingPathCount() const {
        return augmentingPaths;
    }
} //end namespace seamcarve
