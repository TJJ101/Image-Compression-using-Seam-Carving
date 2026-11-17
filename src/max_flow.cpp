#include "seamcarve/max_flow.hpp"
#include <algorithm>
#include <queue>

namespace seamcarve{
    const double EPSILON {1e-9}; // Small value to compare floating point numbers

    /**
    * @brief Creates a graph with numNodes nodes and no edges.
    * @param[in] numNodes	- Number of nodes.
    */
    FlowGraph::FlowGraph(int numNodes) : graph(numNodes) {}

    /**
    * @brief Adds a directed edge from -> to, plus its reverse edge (capacity 0).
    * @param[in] from	- Start node.
    * @param[in] to	- End node.
    * @param[in] capacity	- Capacity of the edge (can be FlowGraph::INF).
    */
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
    const std::vector<FlowEdge>& FlowGraph::EdgesFrom(int node) const {
        return graph[node];
    }
} //end namespace seamcarve
