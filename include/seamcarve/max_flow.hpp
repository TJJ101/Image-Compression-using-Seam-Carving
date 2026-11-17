#pragma once
#include <vector>
#include <limits>

namespace seamcarve {
    //One Edge in the residual graph.
    struct FlowEdge{
        double capacity;    //Capacity of the edge
        double flow;        //Current flow through the edge
        int to;             //Destination node
        int reverse;        //position of the reverse edge inside graph[to]
    };

    class FlowGraph{
        public:
            // Use this capacity for edges that must never be cut.
            static constexpr double INF {std::numeric_limits<double>::infinity()};
            /**
            * @brief Creates a graph with numNodes nodes and no edges.
            * @param[in] numNodes	- Number of nodes.
            */
            FlowGraph(int numNodes);
            /**
            * @brief Adds a directed edge from -> to, plus its reverse edge (capacity 0).
            * @param[in] from	- Start node.
            * @param[in] to	- End node.
            * @param[in] capacity	- Capacity of the edge (can be FlowGraph::INF).
            */
            void AddEdge(int from, int to, double capacity);
            /**
            * @brief Returns the number of nodes.
            * @return Number of nodes.
            */
            int NumNodes() const;
            /**
            * @brief Returns how many augmenting paths the last MaxFlow call used.
            * @return Number of augmenting paths.
            */
            int AugmentingPathCount() const;
            /**
            * @brief Returns the edges leaving a given node.
            * @param[in] node	- The node to get edges from.
            * @return A reference to the vector of edges leaving the node.
            */
            const std::vector<FlowEdge>& EdgesFrom(int node) const;

        private:
            std::vector<std::vector<FlowEdge>> graph;                                                         //Adjacency list representation of the flow network
            int augmentingPaths{0};                                                                             //Number of augmenting paths used in the last MaxFlow call
    };
} //end namespace seamcarve
