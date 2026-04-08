#ifndef CFG_HPP
#define CFG_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "ir.hpp"
#include "trace.hpp"

struct BasicBlock {
    int id;
    std::string function_name;
    std::vector<int> quads;
    std::vector<std::string> instructions;
};

struct CFGEdge {
    int from;
    int to;
    std::string label;
};

struct FunctionFlow {
    std::string name;
    int entry_block = -1;
    std::vector<int> exit_blocks;
};

struct CFG {
    std::vector<BasicBlock> blocks;
    std::vector<CFGEdge> edges;
    std::vector<CFGEdge> call_edges;
    std::vector<FunctionFlow> functions;
    std::string to_dot() const;
};

class CFGBuilder {
    TraceLogger* trace;

    void log(const std::string& message) const;

public:
    explicit CFGBuilder(TraceLogger* trace_logger = nullptr)
        : trace(trace_logger) {}

    CFG build(const IRProgram& ir);
};

#endif
