#pragma once

#include <vector>
#include <string>
#include <atomic>

struct Node {
    int id;
    std::string label;
    std::string kind;  // "axiom", "theorem", "derived"
};

struct Edge {
    int from;
    int to;
    std::string relation;  // "implies", "contradicts", "supports"
};

struct ProofGraph {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

class Reasoner {
public:
    Reasoner() = default;
    void load_axioms(const std::string& path);
    void run();
    void run(std::atomic<bool>& interrupt);
    void save(const std::string& path) const;

private:
    ProofGraph graph;
    int next_id = 0;
};
