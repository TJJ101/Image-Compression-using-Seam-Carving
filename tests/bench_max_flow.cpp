#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "seamcarve/max_flow.hpp"

using seamcarve::FlowGraph;

int main() {
    // Image sizes to test: {width, height}.
    std::vector<std::vector<int>> sizes{ { 10, 10 }, { 25, 25 }, { 50, 50 }, { 75, 75 }, { 100, 100 } };
    std::srand(42);

    std::cout << "size\tnodes\tpaths\ttime_ms\tmax_flow\n";
    for (const std::vector<int>& size : sizes) {
        int cols{ size[0] }, rows{ size[1] };

        // Random energy for every pixel.
        std::vector<std::vector<double>> energy(rows, std::vector<double>(cols));
        for (int i{ 0 }; i < rows; i++)
            for (int j{ 0 }; j < cols; j++) energy[i][j] = std::rand() % 256;

        // Node number of pixel (i, j). The last two nodes are the source and the sink.
        auto node = [cols](int i, int j) { return i * cols + j; };
        int source{ rows * cols }, sink{ rows * cols + 1 };

        FlowGraph g{ rows * cols + 2 };
        for (int i{ 0 }; i < rows; i++) {
            g.AddEdge(source, node(i, 0), FlowGraph::INF);
            g.AddEdge(node(i, cols - 1), sink, FlowGraph::INF);
            for (int j{ 0 }; j + 1 < cols; j++) {
                g.AddEdge(node(i, j), node(i, j + 1), energy[i][j]);
                g.AddEdge(node(i, j + 1), node(i, j), FlowGraph::INF);
            }
            for (int j{ 1 }; j < cols; j++) {
                if (i > 0) g.AddEdge(node(i, j), node(i - 1, j - 1), FlowGraph::INF);
                if (i + 1 < rows) g.AddEdge(node(i, j), node(i + 1, j - 1), FlowGraph::INF);
            }
        }

        // Time only the max-flow part.
        auto start{ std::chrono::steady_clock::now() };
        double flow{ g.MaxFlow(source, sink) };
        auto end{ std::chrono::steady_clock::now() };
        double ms{ std::chrono::duration<double, std::milli>(end - start).count() };

        std::cout << cols << "x" << rows << "\t" << g.NumNodes() << "\t"
                  << g.AugmentingPathCount() << "\t" << ms << "\t" << flow << "\n";
    }
    return 0;
}