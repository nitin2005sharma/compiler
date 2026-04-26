#ifndef OPTIMIZER_HPP
#define OPTIMIZER_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "ir.hpp"
#include "trace.hpp"

struct OptimizationStats {
    size_t constant_folds = 0;
    size_t constant_propagations = 0;
    size_t common_subexpressions_eliminated = 0;
    size_t dead_code_removed = 0;
    size_t dead_stores_removed = 0;
    size_t unreachable_removed = 0;
    size_t loop_simplifications = 0;
    size_t loop_invariant_hoists = 0;
    size_t loop_preheaders_created = 0;
    size_t loop_peels = 0;
    size_t loop_unrolls = 0;
    size_t loop_unswitches = 0;
    size_t strength_reductions = 0;
    size_t induction_variables_optimized = 0;
    std::vector<std::string> explanations;
};

class IROptimizer {
    TraceLogger* trace;

    void log(const std::string& message) const;

public:
    explicit IROptimizer(TraceLogger* trace_logger = nullptr)
        : trace(trace_logger) {}

    IRProgram optimize(const IRProgram& input, OptimizationStats* stats = nullptr) const;
};

#endif
