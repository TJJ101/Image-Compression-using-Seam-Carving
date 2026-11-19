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
    bool FlowGraph::Bfs(int source, int sink, std::vector<int>& parentNode, std::vector<int>& parentEdge) const{
        std::fill(parentNode.begin(), parentNode.end(), -1);
        std::vector<bool> visited(graph.size(), false);
        std::queue<int> q;
        q.push(source);
        visited[source] = true;

        while(!q.empty()){
            int current = q.front();
            q.pop();

            for(int i = 0; i < static_cast<int>(graph[current].size()); ++i){
                const FlowEdge& edge = graph[current][i];
                if(!visited[edge.to] && edge.flow + EPSILON < edge.capacity){
                    visited[edge.to] = true;
                    parentNode[edge.to] = current;
                    parentEdge[edge.to] = i;
                    if(edge.to == sink) return true; // Found a path to sink
                    q.push(edge.to);
                }
            }
        }
        return false; // No path to sink found
    }
    double FlowGraph::MaxFlow(int source, int sink){
        std::vector<int> parentNode(graph.size()), parentEdge(graph.size());
        double maxFlow{0.0};
        augmentingPaths = 0;

        while(Bfs(source, sink, parentNode, parentEdge)){
            // Find the smallest remaining capacity along the path (the bottleneck)
            double pathFlow {INF};
            for(int v = sink; v != source; v = parentNode[v]){
                int current = parentNode[v];
                const FlowEdge& edge = graph[current][parentEdge[v]];
                pathFlow = std::min(pathFlow, edge.capacity - edge.flow);
            }
            if(pathFlow == INF) return INF; // If the path has infinite capacity, the max flow is infinite

            // Push pathflow along the path and subtract it from the reverse edges
            for(int v = sink; v != source; v = parentNode[v]){
                int current = parentNode[v];
                FlowEdge& edge = graph[current][parentEdge[v]];
                edge.flow += pathFlow;
                graph[v][edge.reverse].flow -= pathFlow; // Update reverse edge
            }
            maxFlow += pathFlow;
            augmentingPaths++;
        }
        return maxFlow;
    }
    std::vector<bool> FlowGraph::MinCut(int source) const{
        // After computing max flow, find the nodes reachable from source using BFS on edges that are not full
        std::vector<bool> sourceSide(graph.size(), false);
        std::queue<int> q;
        q.push(source);
        sourceSide[source] = true;

        while(!q.empty()){
            int current = q.front();
            q.pop();

            for(const FlowEdge& edge : graph[current]){
                if(!sourceSide[edge.to] && edge.flow + EPSILON < edge.capacity){
                    sourceSide[edge.to] = true;
                    q.push(edge.to);
                }
            }
        }
        return sourceSide;
    }

} //end namespace seamcarve
