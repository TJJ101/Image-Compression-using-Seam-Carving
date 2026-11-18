#include <stdexcept>
#include "seamcarve/seam_finders.hpp"
#include "seamcarve/seam_graphcut.hpp"

namespace seamcarve {
    SeamGraph BuildSeamGraph(const cv::Mat& energy) {
        if (energy.empty() || energy.type() != CV_64F)
            throw std::invalid_argument("BuildSeamGraph: energy map is empty or not CV_64F");

        const int rows{ energy.rows }, cols{ energy.cols };
        SeamGraph seamGraph{ FlowGraph{ rows * cols + 2 }, rows * cols, rows * cols + 1 };
        FlowGraph& g{ seamGraph.graph };

        for (int row{ 0 }; row < rows; ++row) {
            const double* energyRow{ energy.ptr<double>(row) };
            g.AddEdge(seamGraph.source, PixelNode(row, 0, cols), FlowGraph::INF);

            for (int col{ 0 }; col < cols; ++col) {
                const int node{ PixelNode(row, col, cols) };

                // Cutting this edge removes the pixel; the last column cuts its edge to the sink
                if (col + 1 < cols) {
                    g.AddEdge(node, node + 1, energyRow[col]);
                    g.AddEdge(node + 1, node, FlowGraph::INF);
                } else {
                    g.AddEdge(node, seamGraph.sink, energyRow[col]);
                }

                // Seam column may only shift by one between neighbouring rows
                if (col > 0) {
                    if (row > 0) g.AddEdge(node, PixelNode(row - 1, col - 1, cols), FlowGraph::INF);
                    if (row + 1 < rows) g.AddEdge(node, PixelNode(row + 1, col - 1, cols), FlowGraph::INF);
                }
            }
        }
        return seamGraph;
    }

    Seam FindSeamGraphCut(const cv::Mat& /*energy*/) {
        // TODO(Task 4): build s-t graph, run max-flow (e.g. Edmonds-Karp), BFS on residual graph, read seam.
        throw std::logic_error("FindSeamGraphCut: not implemented");
    }
}  // namespace seamcarve
