#pragma once
#include <vector>
#include <opencv2/core.hpp>
#include "seamcarve/max_flow.hpp"
#include "seamcarve/types.hpp"

namespace seamcarve {
    // The flow network for one vertical seam. Pixel (row, col) is node row * cols + col,
    // followed by the source and the sink.
    struct SeamGraph {
        FlowGraph graph;
        int source;
        int sink;
    };

    /**
    * @brief Returns the node number of a pixel in the seam graph.
    * @param[in] row	- Pixel row.
    * @param[in] col	- Pixel column.
    * @param[in] cols	- Width of the energy map.
    * @return row * cols + col.
    */
    inline int PixelNode(int row, int col, int cols) { return row * cols + col; }

    /**
    * @brief Builds the flow network whose minimum s-t cut is the minimum-energy vertical seam.
    * Every pixel has an edge to its right neighbour (to the sink for the last column) with the
    * pixel's energy as capacity, so cutting it removes that pixel. The source feeds the first
    * column. Infinite edges back to the left neighbour make the cut cross each row once, and
    * infinite diagonal edges to the left neighbours above and below keep the seam connected.
    * @param[in] energy	- Energy map (CV_64F, non-negative).
    * @return The graph with its source and sink nodes.
    * @throws std::invalid_argument If the energy map is empty or not CV_64F.
    */
    SeamGraph BuildSeamGraph(const cv::Mat& energy);

    /**
    * @brief Reads the vertical seam from the source side of a minimum cut of the seam graph.
    * In every row the source side must be a run of pixels starting at column 0; the last pixel of
    * that run is the one removed. Neighbouring rows must differ by at most one column.
    * @param[in] sourceSide	- sourceSide[v] is true if node v is on the source side (FlowGraph::MinCut).
    * @param[in] rows	- Height of the energy map.
    * @param[in] cols	- Width of the energy map.
    * @return One column index per row.
    * @throws std::invalid_argument If sourceSide has fewer than rows * cols entries.
    * @throws std::logic_error If the cut does not describe a valid seam.
    */
    Seam ReadSeamFromCut(const std::vector<bool>& sourceSide, int rows, int cols);

}  // namespace seamcarve
