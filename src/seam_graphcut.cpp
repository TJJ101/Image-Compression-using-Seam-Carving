#include <cstdlib>
#include <stdexcept>
#include <string>
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

    Seam ReadSeamFromCut(const std::vector<bool>& sourceSide, int rows, int cols) {
        if (static_cast<int>(sourceSide.size()) < rows * cols)
            throw std::invalid_argument("ReadSeamFromCut: sourceSide is smaller than the image");

        Seam seam(rows);
        for (int row{ 0 }; row < rows; ++row) {
            int runLength{ 0 };
            while (runLength < cols && sourceSide[PixelNode(row, runLength, cols)]) ++runLength;

            if (runLength == 0)
                throw std::logic_error("ReadSeamFromCut: row " + std::to_string(row) + " has no pixel on the source side");
            for (int col{ runLength }; col < cols; ++col) {
                if (sourceSide[PixelNode(row, col, cols)])
                    throw std::logic_error("ReadSeamFromCut: row " + std::to_string(row) + " is cut more than once");
            }

            seam[row] = runLength - 1;
            if (row > 0 && std::abs(seam[row] - seam[row - 1]) > 1)
                throw std::logic_error("ReadSeamFromCut: seam jumps more than one column at row " + std::to_string(row));
        }
        return seam;
    }

    Seam FindSeamGraphCut(const cv::Mat& energy) {
        SeamGraph seamGraph{ BuildSeamGraph(energy) };
        seamGraph.graph.MaxFlow(seamGraph.source, seamGraph.sink);
        return ReadSeamFromCut(seamGraph.graph.MinCut(seamGraph.source), energy.rows, energy.cols);
    }
}  // namespace seamcarve
