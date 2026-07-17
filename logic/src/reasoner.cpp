#include "reasoner.h"
#include <fstream>
#include <sstream>
#include <chrono>

void Reasoner::load_axioms(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        Node axiom{next_id++, line, "axiom"};
        graph.nodes.push_back(axiom);
    }
}

void Reasoner::run() {
    // TODO: actual inference engine — apply rules, derive new nodes/edges
    Node placeholder{next_id++, "derived: work in progress", "derived"};
    graph.nodes.push_back(placeholder);
    if (graph.nodes.size() >= 2) {
        graph.edges.push_back({0, (int)graph.nodes.size() - 1, "implies"});
    }
}

void Reasoner::run(std::atomic<bool>& interrupt) {
    auto last_check = std::chrono::steady_clock::now();
    int rule_checks = 0;

    while (!interrupt) {
        // TODO: real search loop — fire rules one step at a time
        run(); // placeholder: one step of inference
        rule_checks++;

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_check).count();

        if (rule_checks % 100 == 0 || elapsed_ms >= 1000) {
            if (interrupt) break;
            last_check = now;
        }

        // stop after one step for now (stub)
        break;
    }
}

void Reasoner::save(const std::string& path) const {
    std::ofstream out(path);
    out << "{\n  \"nodes\": [\n";
    for (size_t i = 0; i < graph.nodes.size(); ++i) {
        const auto& n = graph.nodes[i];
        out << "    {\"id\":" << n.id
            << ",\"label\":\"" << n.label
            << "\",\"kind\":\"" << n.kind << "\"}";
        if (i + 1 < graph.nodes.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n  \"edges\": [\n";
    for (size_t i = 0; i < graph.edges.size(); ++i) {
        const auto& e = graph.edges[i];
        out << "    {\"from\":" << e.from
            << ",\"to\":" << e.to
            << ",\"relation\":\"" << e.relation << "\"}";
        if (i + 1 < graph.edges.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
}
