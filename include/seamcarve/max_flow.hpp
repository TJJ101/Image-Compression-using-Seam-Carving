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
            /**
            * @brief Computes the maximum flow from source to sink.
            * @param[in] source	- The source node.
            * @param[in] sink	- The sink node.
            * @return The maximum flow value.
            */
            double MaxFlow(int source, int sink);
            /**
            * @brief Call after MaxFlow. Finds the nodes on the source side of the minimum cut:
            * every node the source can still reach using edges that are not full.
            * @param[in] source	- Source node.
            * @return sourceSide[v] is true if node v is on the source side.
            */
            std::vector<bool> MinCut(int source) const; 

        private:
            /**
            * @brief BFS from source over edges that still have room for more flow.
            * @param[in] source	- Start node.
            * @param[in] sink	- Node we are looking for.
            * @param[out] parentNode	- parentNode[v] = node we came from to reach v (-1 if not reached).
            * @param[out] parentEdge	- parentEdge[v] = which edge of parentNode[v] we used.
            * @return True if the sink was reached.
            */
            bool Bfs(int source, int sink, std::vector<int>& parentNode, std::vector<int>& parentEdge) const;
            std::vector<std::vector<FlowEdge>> graph;                                                         //Adjacency list representation of the flow network
            int augmentingPaths{0};                                                                             //Number of augmenting paths used in the last MaxFlow call
    };
} //end namespace seamcarve
