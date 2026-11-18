#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "seamcarve/seam_graphcut.hpp"

using seamcarve::BuildSeamGraph;
using seamcarve::FlowEdge;
using seamcarve::FlowGraph;
using seamcarve::PixelNode;
using seamcarve::ReadSeamFromCut;
using seamcarve::Seam;
using seamcarve::SeamGraph;

int failures{ 0 };

void Check(const std::string& name, bool ok) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    if (!ok) failures++;
}

// Energy map of the given size, filled row by row from values.
cv::Mat MakeEnergy(int rows, int cols, std::vector<double> values) {
    return cv::Mat(rows, cols, CV_64F, values.data()).clone();
}

// Capacity of the edge from -> to, or -1 if there is none. Reverse edges (capacity 0) are
// skipped, so the energies in these tests are all positive.
double Capacity(const FlowGraph& g, int from, int to) {
    for (const FlowEdge& edge : g.EdgesFrom(from))
        if (edge.to == to && edge.capacity > 0) return edge.capacity;
    return -1;
}

// Number of edges added with AddEdge (reverse edges not counted).
int CountEdges(const FlowGraph& g) {
    int count{ 0 };
    for (int node{ 0 }; node < g.NumNodes(); ++node)
        for (const FlowEdge& edge : g.EdgesFrom(node))
            if (edge.capacity > 0) count++;
    return count;
}

void TestTwoByThree() {
    // 1 2 3
    // 4 5 6
    const cv::Mat energy{ MakeEnergy(2, 3, { 1, 2, 3, 4, 5, 6 }) };
    const SeamGraph sg{ BuildSeamGraph(energy) };
    const FlowGraph& g{ sg.graph };
    auto node = [](int row, int col) { return PixelNode(row, col, 3); };

    Check("2x3: 6 pixels + source + sink", g.NumNodes() == 8);
    Check("2x3: source and sink are the last two nodes", sg.source == 6 && sg.sink == 7);

    Check("2x3: source -> (0,0) is INF", Capacity(g, sg.source, node(0, 0)) == FlowGraph::INF);
    Check("2x3: source -> (1,0) is INF", Capacity(g, sg.source, node(1, 0)) == FlowGraph::INF);
    Check("2x3: source only feeds the first column", Capacity(g, sg.source, node(0, 1)) == -1);

    Check("2x3: (0,0) -> (0,1) has energy 1", Capacity(g, node(0, 0), node(0, 1)) == 1);
    Check("2x3: (1,1) -> (1,2) has energy 5", Capacity(g, node(1, 1), node(1, 2)) == 5);
    Check("2x3: (0,1) -> (0,0) is INF", Capacity(g, node(0, 1), node(0, 0)) == FlowGraph::INF);
    Check("2x3: (1,2) -> (1,1) is INF", Capacity(g, node(1, 2), node(1, 1)) == FlowGraph::INF);

    // The last column must be removable, so its edge to the sink carries its energy
    Check("2x3: (0,2) -> sink has energy 3", Capacity(g, node(0, 2), sg.sink) == 3);
    Check("2x3: (1,2) -> sink has energy 6", Capacity(g, node(1, 2), sg.sink) == 6);
    Check("2x3: only the last column reaches the sink", Capacity(g, node(0, 1), sg.sink) == -1);

    Check("2x3: (0,1) -> (1,0) is INF", Capacity(g, node(0, 1), node(1, 0)) == FlowGraph::INF);
    Check("2x3: (1,1) -> (0,0) is INF", Capacity(g, node(1, 1), node(0, 0)) == FlowGraph::INF);
    Check("2x3: (0,2) -> (1,1) is INF", Capacity(g, node(0, 2), node(1, 1)) == FlowGraph::INF);
    Check("2x3: (1,2) -> (0,1) is INF", Capacity(g, node(1, 2), node(0, 1)) == FlowGraph::INF);
    Check("2x3: no diagonal out of the first column", Capacity(g, node(0, 0), node(1, 0)) == -1);
    Check("2x3: no diagonal to the right", Capacity(g, node(0, 0), node(1, 1)) == -1);

    // 2 source + 4 right + 4 back + 2 sink + 4 diagonal
    Check("2x3: 16 edges in total", CountEdges(g) == 16);
    bool sinkHasOutEdge{ false };
    for (const FlowEdge& edge : g.EdgesFrom(sg.sink))
        if (edge.capacity > 0) sinkHasOutEdge = true;
    Check("2x3: nothing leaves the sink", !sinkHasOutEdge);
}

void TestOneColumn() {
    const cv::Mat energy{ MakeEnergy(3, 1, { 7, 8, 9 }) };
    const SeamGraph sg{ BuildSeamGraph(energy) };
    const FlowGraph& g{ sg.graph };

    Check("3x1: 3 pixels + source + sink", g.NumNodes() == 5);
    Check("3x1: (1,0) -> sink has energy 8", Capacity(g, PixelNode(1, 0, 1), sg.sink) == 8);
    Check("3x1: no diagonals", Capacity(g, PixelNode(1, 0, 1), PixelNode(0, 0, 1)) == -1);
    Check("3x1: 6 edges in total", CountEdges(g) == 6);
}

void TestOneRow() {
    const cv::Mat energy{ MakeEnergy(1, 3, { 7, 8, 9 }) };
    const SeamGraph sg{ BuildSeamGraph(energy) };
    const FlowGraph& g{ sg.graph };

    Check("1x3: (0,1) -> (0,2) has energy 8", Capacity(g, PixelNode(0, 1, 3), PixelNode(0, 2, 3)) == 8);
    Check("1x3: (0,2) -> sink has energy 9", Capacity(g, PixelNode(0, 2, 3), sg.sink) == 9);
    Check("1x3: 6 edges in total", CountEdges(g) == 6);
}

void TestInvalidInput() {
    auto throws = [](const cv::Mat& energy) {
        try {
            BuildSeamGraph(energy);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    Check("empty energy map is rejected", throws(cv::Mat{}));
    Check("non-CV_64F energy map is rejected", throws(cv::Mat(2, 2, CV_32F, cv::Scalar(1))));
}

// Source side of a cut that removes seam[row] in every row: columns 0..seam[row] plus the
// source node. Sizes and values are not checked, so broken cuts can be built from it too.
std::vector<bool> MakeCut(int rows, int cols, const Seam& seam) {
    std::vector<bool> sourceSide(rows * cols + 2, false);
    for (int row{ 0 }; row < rows; ++row)
        for (int col{ 0 }; col <= seam[row]; ++col) sourceSide[PixelNode(row, col, cols)] = true;
    sourceSide[rows * cols] = true;
    return sourceSide;
}

// True if ReadSeamFromCut throws the given exception type.
template <typename Exception>
bool ReadThrows(const std::vector<bool>& sourceSide, int rows, int cols) {
    try {
        ReadSeamFromCut(sourceSide, rows, cols);
    } catch (const Exception&) {
        return true;
    }
    return false;
}

void TestReadSeam() {
    const Seam middle{ 1, 2, 1 };
    Check("3x4: seam {1,2,1} is read back", ReadSeamFromCut(MakeCut(3, 4, middle), 3, 4) == middle);

    const Seam first{ 0, 0, 0 };
    Check("3x4: seam in the first column is read back", ReadSeamFromCut(MakeCut(3, 4, first), 3, 4) == first);

    const Seam last{ 3, 3, 2 };
    Check("3x4: seam in the last column is read back", ReadSeamFromCut(MakeCut(3, 4, last), 3, 4) == last);

    Check("1x1: the only pixel is the seam", ReadSeamFromCut(MakeCut(1, 1, { 0 }), 1, 1) == Seam{ 0 });
    Check("1x3: single row is read back", ReadSeamFromCut(MakeCut(1, 3, { 2 }), 1, 3) == Seam{ 2 });
    Check("3x1: single column is read back", ReadSeamFromCut(MakeCut(3, 1, first), 3, 1) == first);
}

void TestReadSeamInvalidCut() {
    std::vector<bool> gap{ MakeCut(3, 4, { 1, 1, 1 }) };
    gap[PixelNode(1, 3, 4)] = true;
    Check("row cut more than once is rejected", ReadThrows<std::logic_error>(gap, 3, 4));

    std::vector<bool> emptyRow{ MakeCut(3, 4, { 1, 1, 1 }) };
    emptyRow[PixelNode(2, 0, 4)] = false;
    emptyRow[PixelNode(2, 1, 4)] = false;
    Check("row with no source-side pixel is rejected", ReadThrows<std::logic_error>(emptyRow, 3, 4));

    Check("seam jumping two columns is rejected",
          ReadThrows<std::logic_error>(MakeCut(3, 4, { 0, 2, 2 }), 3, 4));

    Check("sourceSide smaller than the image is rejected",
          ReadThrows<std::invalid_argument>(std::vector<bool>(5, true), 2, 3));
}

int main() {
    TestTwoByThree();
    TestOneColumn();
    TestOneRow();
    TestInvalidInput();
    TestReadSeam();
    TestReadSeamInvalidCut();
    std::cout << "Total failures: " << failures << std::endl;
    return failures == 0 ? 0 : 1;
}
