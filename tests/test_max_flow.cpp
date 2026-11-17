#include <iostream>
#include <string>
#include <cmath>
#include "seamcarve/max_flow.hpp"

using seamcarve::FlowEdge;
using seamcarve::FlowGraph;

int failures{0};

void Check(const std::string& name, bool ok){
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    if(!ok) failures++;
}

bool Near(double a, double b) {
    return std::fabs(a - b) < 1e-6;
}

void TestFlowGraph(){
    FlowGraph g{ 3 };
    Check("NumNodes is 3", g.NumNodes() == 3);
    Check("no augmenting paths yet", g.AugmentingPathCount() == 0);
    Check("new graph has no edges", g.EdgesFrom(0).empty());

    g.AddEdge(0, 1, 5.0);
    g.AddEdge(0, 2, 7.5);
    g.AddEdge(1, 2, FlowGraph::INF);
    g.AddEdge(2, 2, 9.0);  // self-loop: should be ignored

    // Each AddEdge stores 2 edges: the real one and its reverse.
    Check("node 0 has 2 edges", g.EdgesFrom(0).size() == 2);
    Check("node 1 has 2 edges (reverse of 0->1, real 1->2)", g.EdgesFrom(1).size() == 2);
    Check("node 2 has 2 edges (reverses only, self-loop ignored)", g.EdgesFrom(2).size() == 2);

    // The real edge 0 -> 1
    const FlowEdge& e{ g.EdgesFrom(0)[0] };
    Check("0->1 goes to node 1", e.to == 1);
    Check("0->1 has capacity 5", e.capacity == 5.0);
    Check("0->1 starts with 0 flow", e.flow == 0.0);

    // Its reverse edge 1 -> 0 must have capacity 0 and point back to node 0
    const FlowEdge& rev{ g.EdgesFrom(1)[e.reverse] };
    Check("reverse of 0->1 goes to node 0", rev.to == 0);
    Check("reverse edge has capacity 0", rev.capacity == 0.0);
    Check("reverse of the reverse is 0->1 again", &g.EdgesFrom(0)[rev.reverse] == &e);

    // INF edge
    Check("1->2 has INF capacity", g.EdgesFrom(1)[1].capacity == FlowGraph::INF);
}

void TestEdmondsKarp() {
    // Two paths s->a->t and s->b->t, each can carry 1. Answer: 2.
    FlowGraph g{ 4 };   // s=0, a=1, b=2, t=3
    g.AddEdge(0, 1, 1);
    g.AddEdge(0, 2, 1);
    g.AddEdge(1, 3, 1);
    g.AddEdge(2, 3, 1);
    Check("max flow is 2", g.MaxFlow(0, 3) == 2.0);
    Check("used 2 augmenting paths", g.AugmentingPathCount() == 2);

    std::vector<bool> side{ g.MinCut(0) };
    Check("source is on source side", side[0]);
    Check("sink is NOT on source side", !side[3]);
}
// =====================================================================================
// Step 3: Test on small textbook graphs where the answer is known
// Graphs from GeeksforGeeks "Ford-Fulkerson Algorithm for Maximum Flow Problem".
// GfG numbers vertices from 1 to V, so we make V + 1 nodes and simply never use node 0.
// =====================================================================================

// Builds a graph from a GfG-style edge list {from, to, capacity} and returns the max flow
// from vertex 1 (source) to vertex V (sink), exactly like the GfG code.
double GfgMaxFlow(int V, const std::vector<std::vector<int>>& edges) {
    FlowGraph g{ V + 1 };
    for (const std::vector<int>& e : edges) g.AddEdge(e[0], e[1], e[2]);
    return g.MaxFlow(1, V);
}

void TestGfgMainExample() {
    // Code example on the page. Expected output: 20.
    std::vector<std::vector<int>> edges{ { 1, 2, 10 }, { 1, 3, 10 }, { 2, 3, 10 }, { 2, 4, 10 }, { 3, 4, 10 } };
    Check("GfG code example: max flow is 20", Near(GfgMaxFlow(4, edges), 20));
}

void TestGfgExample1() {
    // Paths 1->2->4 (2 units) and 1->3->4 (3 units). Expected output: 5.
    std::vector<std::vector<int>> edges{ { 1, 2, 8 }, { 1, 3, 10 }, { 2, 4, 2 }, { 3, 4, 3 } };
    Check("GfG example 1: max flow is 5", Near(GfgMaxFlow(4, edges), 5));
}

void TestGfgExample2() {
    // Paths 1->2->5 (2), 1->2->4->5 (3), 1->3->4->5 (2). Expected output: 7.
    std::vector<std::vector<int>> edges{ { 1, 2, 5 }, { 1, 3, 4 }, { 2, 4, 3 }, { 2, 5, 2 }, { 3, 4, 2 }, { 4, 5, 5 } };
    Check("GfG example 2: max flow is 7", Near(GfgMaxFlow(5, edges), 7));
}

void TestGfgReverseEdge() {
    // GfG residual-graph idea: edge A->B with capacity 10, send 6 units through it.
    // Afterwards the forward edge has 4 left and the reverse edge B->A has 6 (can undo 6).
    FlowGraph g{ 3 };          // S=0, A=1, B=2
    g.AddEdge(0, 1, 6);        // S->A only lets 6 units in
    g.AddEdge(1, 2, 10);       // A->B, capacity 10
    Check("GfG reverse-edge example: max flow is 6", Near(g.MaxFlow(0, 2), 6));

    const FlowEdge& ab{ g.EdgesFrom(1)[1] };            // A->B (index 0 is the reverse of S->A)
    const FlowEdge& ba{ g.EdgesFrom(2)[ab.reverse] };   // its reverse edge B->A
    Check("A->B has 4 left (10 - 6)", Near(ab.capacity - ab.flow, 4));
    Check("B->A reverse edge has 6 (can undo 6)", Near(ba.capacity - ba.flow, 6));
}
void TestSpecialCases() {
    FlowGraph noPath{ 3 };
    noPath.AddEdge(0, 1, 5);  // node 2 (the sink) cannot be reached
    Check("sink not reachable: max flow is 0", Near(noPath.MaxFlow(0, 2), 0));

    FlowGraph withInf{ 4 };
    withInf.AddEdge(0, 1, FlowGraph::INF);
    withInf.AddEdge(1, 2, 2.5);
    withInf.AddEdge(2, 3, FlowGraph::INF);
    Check("INF edges with one finite edge: max flow is 2.5", Near(withInf.MaxFlow(0, 3), 2.5));
}


int main(){
    TestFlowGraph();
    TestEdmondsKarp();
    TestGfgMainExample();   // Step 3
    TestGfgExample1();
    TestGfgExample2();
    TestGfgReverseEdge();
    std::cout << "Total failures: " << failures << std::endl;
    return failures == 0 ? 0 : 1;
}
