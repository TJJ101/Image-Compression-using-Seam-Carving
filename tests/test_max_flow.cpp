#include <iostream>
#include <string>
#include "seamcarve/max_flow.hpp"

using seamcarve::FlowEdge;
using seamcarve::FlowGraph;

int failures{0};

void Check(const std::string& name, bool ok){
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    if(!ok) failures++;
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

int main(){
    TestFlowGraph();
    std::cout << (failures == 0 ? "All tests passed.\n" : "Some tests FAILED.\n");
    return failures == 0 ? 0 : 1;
}