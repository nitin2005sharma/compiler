#include "optimizer.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cfg.hpp"

namespace {

using ConstantMap = std::unordered_map<std::string, int>;
using NameSet = std::unordered_set<std::string>;
using BlockSet = std::unordered_set<int>;

struct FunctionSegment {
    size_t start = 0;
    size_t end = 0;
    std::string name;
};

struct ExprKey {
    std::string op;
    std::string a;
    std::string b;

    bool operator==(const ExprKey& other) const {
        return op == other.op && a == other.a && b == other.b;
    }
};

struct ExprKeyHash {
    size_t operator()(const ExprKey& key) const {
        size_t seed = std::hash<std::string>{}(key.op);
        seed ^= std::hash<std::string>{}(key.a) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<std::string>{}(key.b) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct LoopInfo {
    int header_block = -1;
    int latch_block = -1;
    BlockSet blocks;
    std::string header_label;
    std::string exit_label;
    size_t header_index = 0;
    size_t branch_index = 0;
    size_t backedge_index = 0;
    size_t exit_index = 0;
    bool has_fallthrough_entry = false;
};

struct UnswitchCandidate {
    size_t branch_index = 0;
    size_t else_label_index = 0;
    size_t join_goto_index = 0;
    size_t join_label_index = 0;
};

class GeneratedLabelFactory {
    int next_id = 0;

public:
    explicit GeneratedLabelFactory(const std::vector<Quad>& quads) {
        for (const auto& quad : quads) {
            if (quad.op == "label" && quad.res.size() > 1 && quad.res[0] == 'L') {
                bool numeric = true;
                for (size_t index = 1; index < quad.res.size(); ++index) {
                    if (!std::isdigit(static_cast<unsigned char>(quad.res[index]))) {
                        numeric = false;
                        break;
                    }
                }
                if (numeric) {
                    next_id = std::max(next_id, std::stoi(quad.res.substr(1)) + 1);
                }
            }
        }
    }

    std::string next() {
        return "L" + std::to_string(next_id++);
    }
};

class GeneratedTempFactory {
    int next_id = 0;

public:
    explicit GeneratedTempFactory(const std::vector<Quad>& quads) {
        for (const auto& quad : quads) {
            for (const std::string& value : {quad.a, quad.b, quad.res}) {
                if (value.size() > 1 && value[0] == 't') {
                    bool numeric = true;
                    for (size_t index = 1; index < value.size(); ++index) {
                        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
                            numeric = false;
                            break;
                        }
                    }
                    if (numeric) {
                        next_id = std::max(next_id, std::stoi(value.substr(1)) + 1);
                    }
                }
            }
        }
    }

    std::string next() {
        return "t" + std::to_string(next_id++);
    }
};

void record_explanation(OptimizationStats& stats, const std::string& message) {
    if (stats.explanations.size() < 160) {
        stats.explanations.push_back(message);
    }
}

bool is_generated_label(const std::string& label) {
    if (label.size() < 2 || label[0] != 'L') {
        return false;
    }

    for (size_t index = 1; index < label.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(label[index]))) {
            return false;
        }
    }

    return true;
}

bool is_integer_literal(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    size_t start = value[0] == '-' ? 1 : 0;
    if (start == value.size()) {
        return false;
    }

    for (size_t index = start; index < value.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }

    return true;
}

int parse_integer(const std::string& value) {
    return std::stoi(value);
}

bool is_identifier_name(const std::string& value) {
    if (value.empty() || is_integer_literal(value)) {
        return false;
    }

    if (!(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) {
        return false;
    }

    for (size_t index = 1; index < value.size(); ++index) {
        char ch = value[index];
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
            return false;
        }
    }

    return true;
}

bool is_temporary_name(const std::string& value) {
    if (value.size() < 2 || value[0] != 't') {
        return false;
    }

    for (size_t index = 1; index < value.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }

    return true;
}

bool is_binary_operator(const std::string& op) {
    static const std::unordered_set<std::string> kBinaryOps = {
        "+", "-", "*", "/", "<", "<=", ">", ">=", "==", "!="
    };
    return kBinaryOps.count(op) > 0;
}

bool is_commutative_binary_operator(const std::string& op) {
    return op == "+" || op == "*" || op == "==" || op == "!=";
}

bool is_cse_candidate(const Quad& quad) {
    return is_binary_operator(quad.op) && !quad.res.empty();
}

bool is_loop_invariant_candidate(const Quad& quad) {
    if (!is_temporary_name(quad.res)) {
        return false;
    }

    if (quad.op == "mov") {
        return true;
    }

    if (!is_binary_operator(quad.op)) {
        return false;
    }

    return quad.op != "/";
}

bool is_removable_dead_definition(const Quad& quad) {
    if (quad.op == "mov") {
        return !quad.res.empty();
    }

    if (quad.res.empty() || !is_temporary_name(quad.res) || quad.op == "call") {
        return false;
    }

    return quad.op == "arg" || is_binary_operator(quad.op);
}

std::optional<int> evaluate_binary(const std::string& op, int left, int right) {
    if (op == "+") {
        return left + right;
    }
    if (op == "-") {
        return left - right;
    }
    if (op == "*") {
        return left * right;
    }
    if (op == "/") {
        if (right == 0) {
            return std::nullopt;
        }
        return left / right;
    }
    if (op == "<") {
        return left < right ? 1 : 0;
    }
    if (op == "<=") {
        return left <= right ? 1 : 0;
    }
    if (op == ">") {
        return left > right ? 1 : 0;
    }
    if (op == ">=") {
        return left >= right ? 1 : 0;
    }
    if (op == "==") {
        return left == right ? 1 : 0;
    }
    if (op == "!=") {
        return left != right ? 1 : 0;
    }
    return std::nullopt;
}

bool same_constant_map(const ConstantMap& left, const ConstantMap& right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (const auto& [name, value] : left) {
        auto it = right.find(name);
        if (it == right.end() || it->second != value) {
            return false;
        }
    }

    return true;
}

ConstantMap intersect_constant_maps(const ConstantMap& left, const ConstantMap& right) {
    ConstantMap merged;
    for (const auto& [name, value] : left) {
        auto it = right.find(name);
        if (it != right.end() && it->second == value) {
            merged[name] = value;
        }
    }
    return merged;
}

std::vector<FunctionSegment> split_into_functions(const IRProgram& ir) {
    std::vector<size_t> starts;
    for (size_t index = 0; index < ir.quads.size(); ++index) {
        if (ir.quads[index].op == "label" && !is_generated_label(ir.quads[index].res)) {
            starts.push_back(index);
        }
    }

    std::vector<FunctionSegment> segments;
    if (starts.empty()) {
        if (!ir.quads.empty()) {
            segments.push_back(FunctionSegment{0, ir.quads.size(), ""});
        }
        return segments;
    }

    if (starts.front() > 0) {
        segments.push_back(FunctionSegment{0, starts.front(), ""});
    }

    for (size_t index = 0; index < starts.size(); ++index) {
        size_t start = starts[index];
        size_t end = index + 1 < starts.size() ? starts[index + 1] : ir.quads.size();
        segments.push_back(FunctionSegment{start, end, ir.quads[start].res});
    }

    return segments;
}

IRProgram slice_program(const IRProgram& ir, const FunctionSegment& segment) {
    IRProgram sliced;
    for (size_t index = segment.start; index < segment.end; ++index) {
        sliced.quads.push_back(ir.quads[index]);
    }
    return sliced;
}

std::unordered_map<int, std::vector<int>> predecessor_map(const CFG& cfg) {
    std::unordered_map<int, std::vector<int>> preds;
    for (const auto& edge : cfg.edges) {
        preds[edge.to].push_back(edge.from);
    }
    return preds;
}

std::unordered_map<int, std::vector<int>> successor_map(const CFG& cfg) {
    std::unordered_map<int, std::vector<int>> succs;
    for (const auto& edge : cfg.edges) {
        succs[edge.from].push_back(edge.to);
    }
    return succs;
}

std::vector<int> reachable_blocks(const CFG& cfg, int entry_block) {
    std::vector<int> order;
    if (entry_block < 0) {
        return order;
    }

    std::unordered_map<int, std::vector<int>> succs = successor_map(cfg);
    std::queue<int> worklist;
    std::unordered_set<int> visited;

    worklist.push(entry_block);
    visited.insert(entry_block);

    while (!worklist.empty()) {
        int block_id = worklist.front();
        worklist.pop();
        order.push_back(block_id);

        auto it = succs.find(block_id);
        if (it == succs.end()) {
            continue;
        }

        for (int succ : it->second) {
            if (visited.insert(succ).second) {
                worklist.push(succ);
            }
        }
    }

    return order;
}

std::optional<std::string> defined_name(const Quad& quad) {
    if ((quad.op == "mov" || quad.op == "arg" || quad.op == "call" || is_binary_operator(quad.op)) &&
        !quad.res.empty()) {
        return quad.res;
    }
    return std::nullopt;
}

NameSet used_names(const Quad& quad) {
    NameSet uses;
    auto add_if_name = [&](const std::string& value) {
        if (is_identifier_name(value)) {
            uses.insert(value);
        }
    };

    if (quad.op == "ifFalse" || quad.op == "ret" || quad.op == "mov" || quad.op == "param") {
        add_if_name(quad.a);
    } else if (is_binary_operator(quad.op)) {
        add_if_name(quad.a);
        add_if_name(quad.b);
    }

    return uses;
}

void update_constant_state(const Quad& quad, ConstantMap& constants) {
    auto def = defined_name(quad);
    if (!def.has_value()) {
        return;
    }

    if (quad.op == "mov" && is_integer_literal(quad.a)) {
        constants[*def] = parse_integer(quad.a);
        return;
    }

    if (is_binary_operator(quad.op) && is_integer_literal(quad.a) && is_integer_literal(quad.b)) {
        auto value = evaluate_binary(quad.op, parse_integer(quad.a), parse_integer(quad.b));
        if (value.has_value()) {
            constants[*def] = *value;
            return;
        }
    }

    constants.erase(*def);
}

std::string substitute_operand(const std::string& value, const ConstantMap& constants) {
    if (!is_identifier_name(value)) {
        return value;
    }

    auto it = constants.find(value);
    return it != constants.end() ? std::to_string(it->second) : value;
}

std::string propagate_operand(const std::string& value,
                              const ConstantMap& constants,
                              OptimizationStats& stats,
                              bool* changed) {
    if (!is_identifier_name(value)) {
        return value;
    }

    auto it = constants.find(value);
    if (it == constants.end()) {
        return value;
    }

    ++stats.constant_propagations;
    record_explanation(stats, "Propagated constant '" + std::to_string(it->second) + "' into use of " + value + ".");
    if (changed != nullptr) {
        *changed = true;
    }
    return std::to_string(it->second);
}

bool looks_like_loop_guard(const std::vector<Quad>& quads, size_t index, const std::string& target_label) {
    std::string header_label;
    for (size_t cursor = index; cursor > 0; --cursor) {
        const Quad& candidate = quads[cursor - 1];
        if (candidate.op == "label" && is_generated_label(candidate.res)) {
            header_label = candidate.res;
            break;
        }
    }

    if (header_label.empty()) {
        return false;
    }

    bool saw_exit_label = false;
    for (size_t cursor = index + 1; cursor < quads.size(); ++cursor) {
        const Quad& candidate = quads[cursor];
        if (candidate.op == "label" && candidate.res == target_label) {
            saw_exit_label = true;
            break;
        }
        if (candidate.op == "goto" && candidate.res == header_label) {
            return true;
        }
    }

    return saw_exit_label;
}

ExprKey make_expr_key(const Quad& quad) {
    ExprKey key{quad.op, quad.a, quad.b};
    if (is_commutative_binary_operator(quad.op) && key.a > key.b) {
        std::swap(key.a, key.b);
    }
    return key;
}

std::optional<Quad> simplify_quad_impl(const Quad& quad,
                                       const std::vector<Quad>& quads,
                                       size_t index,
                                       OptimizationStats* stats,
                                       bool* changed) {
    auto mark_change = [&]() {
        if (changed != nullptr) {
            *changed = true;
        }
    };

    if (quad.op == "mov" && quad.a == quad.res) {
        if (stats != nullptr) {
            if (is_temporary_name(quad.res)) {
                ++stats->dead_code_removed;
            } else {
                ++stats->dead_stores_removed;
            }
            record_explanation(*stats, "Removed self-assignment '" + quad.to_string() + "' because it does not change program state.");
        }
        mark_change();
        return std::nullopt;
    }

    if (is_binary_operator(quad.op)) {
        if (is_integer_literal(quad.a) && is_integer_literal(quad.b)) {
            auto value = evaluate_binary(quad.op, parse_integer(quad.a), parse_integer(quad.b));
            if (value.has_value()) {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(
                        *stats,
                        "Constant folded '" + quad.a + " " + quad.op + " " + quad.b +
                            "' into '" + std::to_string(*value) + "' for " + quad.res + ".");
                }
                mark_change();
                return Quad("mov", std::to_string(*value), "", quad.res);
            }
        }

        if (quad.op == "+") {
            if (quad.a == "0") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Simplified addition by zero in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", quad.b, "", quad.res);
            }
            if (quad.b == "0") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Simplified addition by zero in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", quad.a, "", quad.res);
            }
        } else if (quad.op == "-") {
            if (quad.b == "0") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Simplified subtraction by zero in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", quad.a, "", quad.res);
            }
        } else if (quad.op == "*") {
            if (quad.a == "0" || quad.b == "0") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Simplified multiplication by zero in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", "0", "", quad.res);
            }
            if (quad.a == "1") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Simplified multiplication by one in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", quad.b, "", quad.res);
            }
            if (quad.b == "1") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Simplified multiplication by one in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", quad.a, "", quad.res);
            }
        } else if (quad.op == "/") {
            if (quad.b == "1") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Simplified division by one in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", quad.a, "", quad.res);
            }
        } else if (quad.a == quad.b && is_identifier_name(quad.a)) {
            if (quad.op == "==" || quad.op == "<=" || quad.op == ">=") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Folded always-true comparison in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", "1", "", quad.res);
            }
            if (quad.op == "!=" || quad.op == "<" || quad.op == ">") {
                if (stats != nullptr) {
                    ++stats->constant_folds;
                    record_explanation(*stats, "Folded always-false comparison in '" + quad.to_string() + "'.");
                }
                mark_change();
                return Quad("mov", "0", "", quad.res);
            }
        }
    }

    if (quad.op == "ifFalse" && is_integer_literal(quad.a)) {
        mark_change();
        if (parse_integer(quad.a) == 0) {
            if (stats != nullptr && looks_like_loop_guard(quads, index, quad.res)) {
                ++stats->loop_simplifications;
                record_explanation(*stats, "Simplified a constant-false loop guard and redirected control to " + quad.res + ".");
            }
            return Quad("goto", "", "", quad.res);
        }
        return std::nullopt;
    }

    if (quad.op == "goto" && index + 1 < quads.size()) {
        const Quad& next = quads[index + 1];
        if (next.op == "label" && next.res == quad.res) {
            if (stats != nullptr) {
                ++stats->dead_code_removed;
                record_explanation(*stats, "Removed jump to the immediately following label '" + quad.res + "'.");
            }
            mark_change();
            return std::nullopt;
        }
    }

    return quad;
}

std::optional<Quad> simplify_quad(const Quad& quad,
                                  const std::vector<Quad>& quads,
                                  size_t index,
                                  OptimizationStats& stats,
                                  bool* changed) {
    return simplify_quad_impl(quad, quads, index, &stats, changed);
}

std::unordered_map<std::string, size_t> build_label_positions(const IRProgram& ir) {
    std::unordered_map<std::string, size_t> label_positions;
    for (size_t index = 0; index < ir.quads.size(); ++index) {
        if (ir.quads[index].op == "label") {
            label_positions[ir.quads[index].res] = index;
        }
    }
    return label_positions;
}

BlockSet intersect_block_sets(const BlockSet& left, const BlockSet& right) {
    BlockSet merged;
    for (int block_id : left) {
        if (right.count(block_id)) {
            merged.insert(block_id);
        }
    }
    return merged;
}

std::unordered_map<int, BlockSet> compute_dominators(const CFG& cfg,
                                                     int entry_block,
                                                     const std::unordered_map<int, std::vector<int>>& preds) {
    std::unordered_map<int, BlockSet> doms;
    BlockSet all_blocks;
    for (const auto& block : cfg.blocks) {
        all_blocks.insert(block.id);
    }

    for (const auto& block : cfg.blocks) {
        if (block.id == entry_block) {
            doms[block.id] = BlockSet{block.id};
        } else {
            doms[block.id] = all_blocks;
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& block : cfg.blocks) {
            if (block.id == entry_block) {
                continue;
            }

            BlockSet new_dom = all_blocks;
            auto pred_it = preds.find(block.id);
            if (pred_it == preds.end() || pred_it->second.empty()) {
                new_dom.clear();
            } else {
                bool first_pred = true;
                for (int pred : pred_it->second) {
                    if (first_pred) {
                        new_dom = doms[pred];
                        first_pred = false;
                    } else {
                        new_dom = intersect_block_sets(new_dom, doms[pred]);
                    }
                }
            }

            new_dom.insert(block.id);
            if (new_dom != doms[block.id]) {
                doms[block.id] = std::move(new_dom);
                changed = true;
            }
        }
    }

    return doms;
}

std::vector<LoopInfo> find_natural_loops(const IRProgram& local, const CFG& cfg) {
    std::vector<LoopInfo> loops;
    if (local.quads.empty() || cfg.blocks.empty()) {
        return loops;
    }

    std::unordered_map<int, const BasicBlock*> blocks_by_id;
    for (const auto& block : cfg.blocks) {
        blocks_by_id[block.id] = &block;
    }

    int entry_block = !cfg.functions.empty() ? cfg.functions.front().entry_block
                                             : cfg.blocks.front().id;
    std::unordered_map<int, std::vector<int>> preds = predecessor_map(cfg);
    std::unordered_map<int, BlockSet> doms = compute_dominators(cfg, entry_block, preds);
    std::unordered_map<std::string, size_t> label_positions = build_label_positions(local);
    std::unordered_set<int> seen_headers;

    for (const auto& edge : cfg.edges) {
        auto dom_it = doms.find(edge.from);
        if (dom_it == doms.end() || !dom_it->second.count(edge.to)) {
            continue;
        }

        if (seen_headers.count(edge.to)) {
            continue;
        }

        auto header_it = blocks_by_id.find(edge.to);
        auto latch_it = blocks_by_id.find(edge.from);
        if (header_it == blocks_by_id.end() || latch_it == blocks_by_id.end()) {
            continue;
        }

        const BasicBlock& header_block = *header_it->second;
        const BasicBlock& latch_block = *latch_it->second;
        if (header_block.quads.empty() || latch_block.quads.empty()) {
            continue;
        }

        size_t header_index = static_cast<size_t>(header_block.quads.front());
        size_t branch_index = static_cast<size_t>(header_block.quads.back());
        size_t backedge_index = static_cast<size_t>(latch_block.quads.back());
        const Quad& header_quad = local.quads[header_index];

        if (header_quad.op != "label" || !is_generated_label(header_quad.res)) {
            continue;
        }

        if (local.quads[branch_index].op != "ifFalse") {
            continue;
        }

        if (local.quads[backedge_index].op != "goto" ||
            local.quads[backedge_index].res != header_quad.res) {
            continue;
        }

        auto exit_it = label_positions.find(local.quads[branch_index].res);
        if (exit_it == label_positions.end()) {
            continue;
        }

        size_t exit_index = exit_it->second;
        if (!(header_index < branch_index && branch_index < backedge_index && backedge_index < exit_index)) {
            continue;
        }

        BlockSet loop_blocks{edge.to, edge.from};
        std::vector<int> worklist{edge.from};
        while (!worklist.empty()) {
            int block_id = worklist.back();
            worklist.pop_back();

            auto pred_it = preds.find(block_id);
            if (pred_it == preds.end()) {
                continue;
            }

            for (int pred : pred_it->second) {
                if (!loop_blocks.count(pred)) {
                    loop_blocks.insert(pred);
                    if (pred != edge.to) {
                        worklist.push_back(pred);
                    }
                }
            }
        }

        bool contiguous = true;
        for (int block_id : loop_blocks) {
            auto block_it = blocks_by_id.find(block_id);
            if (block_it == blocks_by_id.end() || block_it->second->quads.empty()) {
                contiguous = false;
                break;
            }

            size_t block_start = static_cast<size_t>(block_it->second->quads.front());
            size_t block_end = static_cast<size_t>(block_it->second->quads.back());
            if (block_start < header_index || block_end > backedge_index) {
                contiguous = false;
                break;
            }
        }

        if (!contiguous) {
            continue;
        }

        bool has_fallthrough_entry = false;
        for (const auto& cfg_edge : cfg.edges) {
            if (cfg_edge.to == edge.to && !loop_blocks.count(cfg_edge.from) && cfg_edge.label != "goto") {
                has_fallthrough_entry = true;
                break;
            }
        }

        LoopInfo loop;
        loop.header_block = edge.to;
        loop.latch_block = edge.from;
        loop.blocks = std::move(loop_blocks);
        loop.header_label = header_quad.res;
        loop.exit_label = local.quads[branch_index].res;
        loop.header_index = header_index;
        loop.branch_index = branch_index;
        loop.backedge_index = backedge_index;
        loop.exit_index = exit_index;
        loop.has_fallthrough_entry = has_fallthrough_entry;
        loops.push_back(loop);
        seen_headers.insert(edge.to);
    }

    std::sort(loops.begin(), loops.end(), [](const LoopInfo& left, const LoopInfo& right) {
        return left.header_index > right.header_index;
    });
    return loops;
}

std::vector<Quad> quad_range(const std::vector<Quad>& quads, size_t begin, size_t end) {
    std::vector<Quad> slice;
    if (begin >= end || begin >= quads.size()) {
        return slice;
    }

    end = std::min(end, quads.size());
    slice.reserve(end - begin);
    for (size_t index = begin; index < end; ++index) {
        slice.push_back(quads[index]);
    }
    return slice;
}

void append_quads(std::vector<Quad>& out, const std::vector<Quad>& more) {
    out.insert(out.end(), more.begin(), more.end());
}

std::vector<Quad> clone_quads_with_label_remap(const std::vector<Quad>& source, GeneratedLabelFactory& labels) {
    std::unordered_map<std::string, std::string> label_map;
    for (const auto& quad : source) {
        if (quad.op == "label" && is_generated_label(quad.res)) {
            label_map.emplace(quad.res, labels.next());
        }
    }

    std::vector<Quad> cloned;
    cloned.reserve(source.size());
    for (const auto& quad : source) {
        Quad rewritten = quad;
        if (rewritten.op == "label") {
            auto it = label_map.find(rewritten.res);
            if (it != label_map.end()) {
                rewritten.res = it->second;
            }
        } else if (rewritten.op == "goto" || rewritten.op == "ifFalse") {
            auto it = label_map.find(rewritten.res);
            if (it != label_map.end()) {
                rewritten.res = it->second;
            }
        }
        cloned.push_back(std::move(rewritten));
    }
    return cloned;
}

NameSet collect_loop_defined_names(const IRProgram& local, const LoopInfo& loop) {
    NameSet names;
    for (size_t index = loop.header_index + 1; index <= loop.backedge_index; ++index) {
        auto def = defined_name(local.quads[index]);
        if (def.has_value()) {
            names.insert(*def);
        }
    }
    return names;
}

bool retarget_external_header_jumps(std::vector<Quad>& quads,
                                    const LoopInfo& loop,
                                    const std::string& new_target) {
    bool changed = false;
    for (size_t index = 0; index < quads.size(); ++index) {
        if (index >= loop.header_index && index <= loop.backedge_index) {
            continue;
        }

        Quad& quad = quads[index];
        if ((quad.op == "goto" || quad.op == "ifFalse") && quad.res == loop.header_label) {
            quad.res = new_target;
            changed = true;
        }
    }
    return changed;
}

bool region_contains_op(const std::vector<Quad>& quads,
                        size_t begin,
                        size_t end,
                        const std::string& op) {
    end = std::min(end, quads.size());
    for (size_t index = begin; index < end; ++index) {
        if (quads[index].op == op) {
            return true;
        }
    }
    return false;
}

bool region_has_labels(const std::vector<Quad>& quads, size_t begin, size_t end) {
    return region_contains_op(quads, begin, end, "label");
}

bool region_has_body_control_flow(const std::vector<Quad>& quads, size_t begin, size_t end) {
    end = std::min(end, quads.size());
    for (size_t index = begin; index < end; ++index) {
        const std::string& op = quads[index].op;
        if (op == "label" || op == "goto" || op == "ifFalse" || op == "ret") {
            return true;
        }
    }
    return false;
}

size_t count_nonlabel_quads(const std::vector<Quad>& quads, size_t begin, size_t end) {
    size_t count = 0;
    end = std::min(end, quads.size());
    for (size_t index = begin; index < end; ++index) {
        if (quads[index].op != "label") {
            ++count;
        }
    }
    return count;
}

std::optional<UnswitchCandidate> find_unswitch_candidate(const IRProgram& local, const LoopInfo& loop) {
    NameSet loop_defined = collect_loop_defined_names(local, loop);
    std::unordered_map<std::string, size_t> label_positions = build_label_positions(local);

    for (size_t index = loop.branch_index + 1; index < loop.backedge_index; ++index) {
        const Quad& quad = local.quads[index];
        if (quad.op != "ifFalse") {
            continue;
        }

        if (is_identifier_name(quad.a) && loop_defined.count(quad.a)) {
            continue;
        }

        auto else_it = label_positions.find(quad.res);
        if (else_it == label_positions.end()) {
            continue;
        }

        size_t else_label_index = else_it->second;
        if (!(index < else_label_index && else_label_index < loop.backedge_index)) {
            continue;
        }

        if (else_label_index == 0) {
            continue;
        }

        size_t join_goto_index = else_label_index - 1;
        if (local.quads[join_goto_index].op != "goto") {
            continue;
        }

        auto join_it = label_positions.find(local.quads[join_goto_index].res);
        if (join_it == label_positions.end()) {
            continue;
        }

        size_t join_label_index = join_it->second;
        if (!(else_label_index < join_label_index && join_label_index <= loop.backedge_index)) {
            continue;
        }

        return UnswitchCandidate{index, else_label_index, join_goto_index, join_label_index};
    }

    return std::nullopt;
}

std::string format_name_set(const NameSet& names) {
    if (names.empty()) {
        return "{}";
    }

    std::vector<std::string> sorted(names.begin(), names.end());
    std::sort(sorted.begin(), sorted.end());

    std::ostringstream os;
    os << "{";
    for (size_t index = 0; index < sorted.size(); ++index) {
        if (index > 0) {
            os << ", ";
        }
        os << sorted[index];
    }
    os << "}";
    return os.str();
}

IRProgram eliminate_common_subexpressions(const IRProgram& input,
                                          OptimizationStats& stats,
                                          const TraceLogger* trace,
                                          bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        CFG cfg = cfg_builder.build(local);
        std::vector<std::optional<Quad>> rewritten(local.quads.size());

        for (const auto& block : cfg.blocks) {
            std::unordered_map<ExprKey, std::string, ExprKeyHash> available;

            auto invalidate_name = [&](const std::string& name) {
                if (name.empty()) {
                    return;
                }

                std::vector<ExprKey> stale;
                for (const auto& [key, result] : available) {
                    if (key.a == name || key.b == name || result == name) {
                        stale.push_back(key);
                    }
                }
                for (const auto& key : stale) {
                    available.erase(key);
                }
            };

            for (int quad_index : block.quads) {
                Quad quad = local.quads[static_cast<size_t>(quad_index)];
                auto def = defined_name(quad);
                if (def.has_value()) {
                    invalidate_name(*def);
                }

                if (quad.op == "call") {
                    available.clear();
                } else if (is_cse_candidate(quad)) {
                    ExprKey key = make_expr_key(quad);
                    auto it = available.find(key);
                    if (it != available.end() && it->second != quad.res) {
                        std::string original = quad.to_string();
                        quad = Quad("mov", it->second, "", quad.res);
                        ++stats.common_subexpressions_eliminated;
                        record_explanation(
                            stats,
                            "Reused common expression from " + it->second + " for '" + original +
                                "', replacing it with '" + quad.to_string() + "'.");
                        if (changed != nullptr) {
                            *changed = true;
                        }
                    } else {
                        available[key] = quad.res;
                    }
                }

                rewritten[static_cast<size_t>(quad_index)] = quad;
            }
        }

        for (const auto& quad : rewritten) {
            if (quad.has_value()) {
                output.quads.push_back(*quad);
            }
        }
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "local-CSE pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

IRProgram hoist_loop_invariants(const IRProgram& input,
                                OptimizationStats& stats,
                                const TraceLogger* trace,
                                bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        GeneratedLabelFactory labels(local.quads);
        bool transformed = true;
        while (transformed) {
            transformed = false;
            CFG cfg = cfg_builder.build(local);
            std::vector<LoopInfo> loops = find_natural_loops(local, cfg);

            for (const auto& loop : loops) {
                NameSet loop_defined = collect_loop_defined_names(local, loop);
                NameSet hoisted_defs;
                std::vector<size_t> hoist_indices;

                for (size_t index = loop.branch_index + 1; index < loop.backedge_index; ++index) {

                    const Quad& quad = local.quads[index];
                    if (quad.op == "label" || quad.op == "goto" || quad.op == "ifFalse" || quad.op == "ret") {
                        continue;
                    }

                    if (!is_loop_invariant_candidate(quad)) {
                        continue;
                    }

                    auto operand_invariant = [&](const std::string& operand) {
                        return !is_identifier_name(operand) ||
                               !loop_defined.count(operand) ||
                               hoisted_defs.count(operand);
                    };

                    if (!operand_invariant(quad.a) || !operand_invariant(quad.b)) {
                        continue;
                    }

                    hoist_indices.push_back(index);
                    hoisted_defs.insert(quad.res);
                }

                if (hoist_indices.empty()) {
                    continue;
                }

                std::string preheader_label = labels.next();
                std::vector<Quad> rewritten = local.quads;
                retarget_external_header_jumps(rewritten, loop, preheader_label);

                std::unordered_set<size_t> removed(hoist_indices.begin(), hoist_indices.end());
                std::vector<Quad> rebuilt;
                rebuilt.reserve(rewritten.size() + hoist_indices.size() + 1);

                for (size_t index = 0; index < loop.header_index; ++index) {
                    rebuilt.push_back(rewritten[index]);
                }

                rebuilt.push_back(Quad("label", "", "", preheader_label));
                for (size_t index : hoist_indices) {
                    rebuilt.push_back(rewritten[index]);
                    ++stats.loop_invariant_hoists;
                    record_explanation(
                        stats,
                        "Hoisted loop-invariant instruction '" + rewritten[index].to_string() +
                            "' into preheader " + preheader_label + " before loop " + loop.header_label + ".");
                }
                ++stats.loop_preheaders_created;
                record_explanation(stats, "Created loop preheader " + preheader_label + " for loop header " + loop.header_label + ".");

                for (size_t index = loop.header_index; index < rewritten.size(); ++index) {
                    if (!removed.count(index)) {
                        rebuilt.push_back(rewritten[index]);
                    }
                }

                local.quads = std::move(rebuilt);
                if (changed != nullptr) {
                    *changed = true;
                }
                transformed = true;
                break;
            }
        }

        append_quads(output.quads, local.quads);
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "dominance-LICM pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

IRProgram propagate_and_fold(const IRProgram& input,
                             OptimizationStats& stats,
                             const TraceLogger* trace,
                             bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        CFG cfg = cfg_builder.build(local);
        int entry_block = !cfg.functions.empty() ? cfg.functions.front().entry_block
                                                 : (!cfg.blocks.empty() ? cfg.blocks.front().id : -1);
        std::vector<int> reachable = reachable_blocks(cfg, entry_block);
        std::unordered_set<int> reachable_set(reachable.begin(), reachable.end());
        std::unordered_map<int, std::vector<int>> preds = predecessor_map(cfg);

        std::unordered_map<int, ConstantMap> in_state;
        std::unordered_map<int, ConstantMap> out_state;

        bool flow_changed = true;
        while (flow_changed) {
            flow_changed = false;
            for (const auto& block : cfg.blocks) {
                if (!reachable_set.count(block.id)) {
                    continue;
                }

                ConstantMap merged_in;
                bool first_pred = true;
                auto pred_it = preds.find(block.id);
                if (pred_it != preds.end()) {
                    for (int pred : pred_it->second) {
                        if (!reachable_set.count(pred)) {
                            continue;
                        }
                        if (first_pred) {
                            merged_in = out_state[pred];
                            first_pred = false;
                        } else {
                            merged_in = intersect_constant_maps(merged_in, out_state[pred]);
                        }
                    }
                }
                if (first_pred) {
                    merged_in.clear();
                }

                if (!same_constant_map(in_state[block.id], merged_in)) {
                    in_state[block.id] = merged_in;
                    flow_changed = true;
                }

                ConstantMap propagated = merged_in;
                for (int quad_index : block.quads) {
                    Quad rewritten = local.quads[static_cast<size_t>(quad_index)];
                    rewritten.a = substitute_operand(rewritten.a, propagated);
                    rewritten.b = substitute_operand(rewritten.b, propagated);

                    auto simplified = simplify_quad_impl(
                        rewritten,
                        local.quads,
                        static_cast<size_t>(quad_index),
                        nullptr,
                        nullptr);
                    if (simplified.has_value()) {
                        update_constant_state(*simplified, propagated);
                    }
                }

                if (!same_constant_map(out_state[block.id], propagated)) {
                    out_state[block.id] = propagated;
                    flow_changed = true;
                }
            }
        }

        std::vector<std::optional<Quad>> rewritten_quads(local.quads.size());
        for (const auto& block : cfg.blocks) {
            ConstantMap constants = reachable_set.count(block.id) ? in_state[block.id] : ConstantMap{};
            for (int quad_index : block.quads) {
                Quad rewritten = local.quads[static_cast<size_t>(quad_index)];
                if (reachable_set.count(block.id)) {
                    rewritten.a = propagate_operand(rewritten.a, constants, stats, changed);
                    rewritten.b = propagate_operand(rewritten.b, constants, stats, changed);
                    rewritten_quads[static_cast<size_t>(quad_index)] = simplify_quad(
                        rewritten,
                        local.quads,
                        static_cast<size_t>(quad_index),
                        stats,
                        changed);
                } else {
                    rewritten.a = substitute_operand(rewritten.a, constants);
                    rewritten.b = substitute_operand(rewritten.b, constants);
                    rewritten_quads[static_cast<size_t>(quad_index)] = simplify_quad_impl(
                        rewritten,
                        local.quads,
                        static_cast<size_t>(quad_index),
                        nullptr,
                        nullptr);
                }

                if (rewritten_quads[static_cast<size_t>(quad_index)].has_value()) {
                    update_constant_state(*rewritten_quads[static_cast<size_t>(quad_index)], constants);
                }
            }
        }

        for (const auto& quad : rewritten_quads) {
            if (quad.has_value()) {
                output.quads.push_back(*quad);
            }
        }
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "propagation/folding pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

IRProgram unswitch_loops(const IRProgram& input,
                         OptimizationStats& stats,
                         const TraceLogger* trace,
                         bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        GeneratedLabelFactory labels(local.quads);
        bool transformed = true;
        while (transformed) {
            transformed = false;
            CFG cfg = cfg_builder.build(local);
            std::vector<LoopInfo> loops = find_natural_loops(local, cfg);

            for (const auto& loop : loops) {
                auto candidate = find_unswitch_candidate(local, loop);
                if (!candidate.has_value()) {
                    continue;
                }

                std::string unswitched_branch = local.quads[candidate->branch_index].to_string();
                std::vector<Quad> cond_segment = quad_range(local.quads, loop.header_index + 1, loop.branch_index + 1);
                std::vector<Quad> pre_branch = quad_range(local.quads, loop.branch_index + 1, candidate->branch_index);
                std::vector<Quad> then_segment = quad_range(local.quads, candidate->branch_index + 1, candidate->join_goto_index);
                std::vector<Quad> else_segment = quad_range(local.quads, candidate->else_label_index + 1, candidate->join_label_index);
                std::vector<Quad> rest_segment = quad_range(local.quads, candidate->join_label_index + 1, loop.backedge_index);

                std::vector<Quad> true_source;
                append_quads(true_source, pre_branch);
                append_quads(true_source, then_segment);
                append_quads(true_source, rest_segment);

                std::vector<Quad> false_source;
                append_quads(false_source, pre_branch);
                append_quads(false_source, else_segment);
                append_quads(false_source, rest_segment);

                std::string dispatch_false_label = labels.next();
                std::string true_header_label = labels.next();
                std::string false_header_label = labels.next();
                std::vector<Quad> true_body = clone_quads_with_label_remap(true_source, labels);
                std::vector<Quad> false_body = clone_quads_with_label_remap(false_source, labels);

                std::vector<Quad> rebuilt;
                rebuilt.reserve(local.quads.size() + true_body.size() + false_body.size() + 8);

                for (size_t index = 0; index < loop.header_index; ++index) {
                    rebuilt.push_back(local.quads[index]);
                }

                rebuilt.push_back(local.quads[loop.header_index]);
                rebuilt.push_back(Quad("ifFalse", local.quads[candidate->branch_index].a, "", dispatch_false_label));
                rebuilt.push_back(Quad("label", "", "", true_header_label));
                append_quads(rebuilt, cond_segment);
                append_quads(rebuilt, true_body);
                rebuilt.push_back(Quad("goto", "", "", true_header_label));
                rebuilt.push_back(Quad("label", "", "", dispatch_false_label));
                rebuilt.push_back(Quad("label", "", "", false_header_label));
                append_quads(rebuilt, cond_segment);
                append_quads(rebuilt, false_body);
                rebuilt.push_back(Quad("goto", "", "", false_header_label));

                for (size_t index = loop.exit_index; index < local.quads.size(); ++index) {
                    rebuilt.push_back(local.quads[index]);
                }

                local.quads = std::move(rebuilt);
                ++stats.loop_unswitches;
                record_explanation(
                    stats,
                    "Unswitching moved invariant branch '" + unswitched_branch + "' outside loop " + loop.header_label + ".");
                if (changed != nullptr) {
                    *changed = true;
                }
                transformed = true;
                break;
            }
        }

        append_quads(output.quads, local.quads);
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "loop-unswitch pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

IRProgram peel_loops(const IRProgram& input,
                     OptimizationStats& stats,
                     const TraceLogger* trace,
                     bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        GeneratedLabelFactory labels(local.quads);
        bool transformed = true;
        while (transformed) {
            transformed = false;
            CFG cfg = cfg_builder.build(local);
            std::vector<LoopInfo> loops = find_natural_loops(local, cfg);

            for (const auto& loop : loops) {
                size_t body_begin = loop.branch_index + 1;
                size_t body_end = loop.backedge_index;
                size_t body_quads = count_nonlabel_quads(local.quads, body_begin, body_end);

                if (!loop.has_fallthrough_entry ||
                    body_quads == 0 ||
                    body_quads > 10 ||
                    !region_has_labels(local.quads, body_begin, body_end) ||
                    region_contains_op(local.quads, body_begin, body_end, "ret")) {
                    continue;
                }

                std::string peeled_loop_label = labels.next();
                std::vector<Quad> cond_segment = quad_range(local.quads, loop.header_index + 1, loop.branch_index + 1);
                std::vector<Quad> body_copy = clone_quads_with_label_remap(
                    quad_range(local.quads, body_begin, body_end), labels);
                std::vector<Quad> original_body = quad_range(local.quads, body_begin, body_end);

                std::vector<Quad> rebuilt;
                rebuilt.reserve(local.quads.size() + body_copy.size() + cond_segment.size() + 3);

                for (size_t index = 0; index < loop.header_index; ++index) {
                    rebuilt.push_back(local.quads[index]);
                }

                rebuilt.push_back(local.quads[loop.header_index]);
                append_quads(rebuilt, cond_segment);
                append_quads(rebuilt, body_copy);
                rebuilt.push_back(Quad("goto", "", "", peeled_loop_label));
                rebuilt.push_back(Quad("label", "", "", peeled_loop_label));
                append_quads(rebuilt, cond_segment);
                append_quads(rebuilt, original_body);
                rebuilt.push_back(Quad("goto", "", "", peeled_loop_label));

                for (size_t index = loop.exit_index; index < local.quads.size(); ++index) {
                    rebuilt.push_back(local.quads[index]);
                }

                local.quads = std::move(rebuilt);
                ++stats.loop_peels;
                record_explanation(stats, "Peeled the first iteration of loop " + loop.header_label + " to expose branch and constant simplifications.");
                if (changed != nullptr) {
                    *changed = true;
                }
                transformed = true;
                break;
            }
        }

        append_quads(output.quads, local.quads);
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "loop-peeling pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

IRProgram unroll_loops(const IRProgram& input,
                       OptimizationStats& stats,
                       const TraceLogger* trace,
                       bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        GeneratedLabelFactory labels(local.quads);
        bool transformed = true;
        while (transformed) {
            transformed = false;
            CFG cfg = cfg_builder.build(local);
            std::vector<LoopInfo> loops = find_natural_loops(local, cfg);

            for (const auto& loop : loops) {
                size_t body_begin = loop.branch_index + 1;
                size_t body_end = loop.backedge_index;
                size_t body_quads = count_nonlabel_quads(local.quads, body_begin, body_end);

                if (body_quads == 0 ||
                    body_quads > 3 ||
                    region_has_body_control_flow(local.quads, body_begin, body_end)) {
                    continue;
                }

                std::vector<Quad> cond_segment = quad_range(local.quads, loop.header_index + 1, loop.branch_index + 1);
                std::vector<Quad> original_body = quad_range(local.quads, body_begin, body_end);
                std::vector<Quad> duplicated_body = clone_quads_with_label_remap(original_body, labels);

                std::vector<Quad> rebuilt;
                rebuilt.reserve(local.quads.size() + cond_segment.size() + duplicated_body.size() + 1);

                for (size_t index = 0; index < loop.header_index; ++index) {
                    rebuilt.push_back(local.quads[index]);
                }

                rebuilt.push_back(local.quads[loop.header_index]);
                append_quads(rebuilt, cond_segment);
                append_quads(rebuilt, original_body);
                append_quads(rebuilt, cond_segment);
                append_quads(rebuilt, duplicated_body);
                rebuilt.push_back(Quad("goto", "", "", loop.header_label));

                for (size_t index = loop.exit_index; index < local.quads.size(); ++index) {
                    rebuilt.push_back(local.quads[index]);
                }

                local.quads = std::move(rebuilt);
                ++stats.loop_unrolls;
                record_explanation(stats, "Unrolled loop " + loop.header_label + " by duplicating one simple loop body iteration.");
                if (changed != nullptr) {
                    *changed = true;
                }
                transformed = true;
                break;
            }
        }

        append_quads(output.quads, local.quads);
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "loop-unrolling pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

IRProgram reduce_strength_and_induction_variables(const IRProgram& input,
                                                  OptimizationStats& stats,
                                                  const TraceLogger* trace,
                                                  bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        GeneratedLabelFactory labels(local.quads);
        GeneratedTempFactory temps(local.quads);
        bool transformed = true;
        while (transformed) {
            transformed = false;
            CFG cfg = cfg_builder.build(local);
            std::vector<LoopInfo> loops = find_natural_loops(local, cfg);

            for (const auto& loop : loops) {
                size_t body_begin = loop.branch_index + 1;
                size_t body_end = loop.backedge_index;
                if (region_has_body_control_flow(local.quads, body_begin, body_end)) {
                    continue;
                }

                std::string induction_var;
                int step = 0;
                size_t induction_update_index = local.quads.size();
                for (size_t index = body_begin; index + 1 < body_end; ++index) {
                    const Quad& add = local.quads[index];
                    const Quad& assign = local.quads[index + 1];
                    if ((add.op != "+" && add.op != "-") || assign.op != "mov" || assign.a != add.res) {
                        continue;
                    }

                    if (add.op == "+" && add.a == assign.res && is_integer_literal(add.b)) {
                        induction_var = assign.res;
                        step = parse_integer(add.b);
                    } else if (add.op == "+" && add.b == assign.res && is_integer_literal(add.a)) {
                        induction_var = assign.res;
                        step = parse_integer(add.a);
                    } else if (add.op == "-" && add.a == assign.res && is_integer_literal(add.b)) {
                        induction_var = assign.res;
                        step = -parse_integer(add.b);
                    }

                    if (!induction_var.empty() && step != 0) {
                        induction_update_index = index + 1;
                        break;
                    }
                }

                if (induction_var.empty() || step == 0) {
                    continue;
                }

                struct Reduction {
                    size_t index = 0;
                    std::string result;
                    std::string factor;
                    int delta = 0;
                    std::string reduced_temp;
                };

                std::vector<Reduction> reductions;
                for (size_t index = body_begin; index < induction_update_index; ++index) {
                    const Quad& quad = local.quads[index];
                    if (quad.op != "*" || quad.res.empty()) {
                        continue;
                    }

                    std::string factor;
                    if (quad.a == induction_var && is_integer_literal(quad.b)) {
                        factor = quad.b;
                    } else if (quad.b == induction_var && is_integer_literal(quad.a)) {
                        factor = quad.a;
                    }

                    if (factor.empty()) {
                        continue;
                    }

                    int delta = parse_integer(factor) * step;
                    if (delta == 0) {
                        continue;
                    }

                    reductions.push_back(Reduction{index, quad.res, factor, delta, temps.next()});
                }

                if (reductions.empty()) {
                    continue;
                }

                std::string preheader_label = labels.next();
                std::vector<Quad> rewritten = local.quads;
                retarget_external_header_jumps(rewritten, loop, preheader_label);

                std::vector<Quad> rebuilt;
                rebuilt.reserve(rewritten.size() + reductions.size() * 4 + 1);

                for (size_t index = 0; index < loop.header_index; ++index) {
                    rebuilt.push_back(rewritten[index]);
                }

                rebuilt.push_back(Quad("label", "", "", preheader_label));
                for (const auto& reduction : reductions) {
                    rebuilt.push_back(Quad("*", induction_var, reduction.factor, reduction.reduced_temp));
                }

                for (size_t index = loop.header_index; index < rewritten.size(); ++index) {
                    auto reduction_it = std::find_if(
                        reductions.begin(),
                        reductions.end(),
                        [&](const Reduction& reduction) { return reduction.index == index; });

                    if (reduction_it != reductions.end()) {
                        rebuilt.push_back(Quad("mov", reduction_it->reduced_temp, "", reduction_it->result));
                        ++stats.strength_reductions;
                        record_explanation(
                            stats,
                            "Strength reduced loop multiply '" + rewritten[index].to_string() +
                                "' by maintaining " + reduction_it->reduced_temp +
                                " with additions inside loop " + loop.header_label + ".");
                        continue;
                    }

                    if (index == loop.backedge_index) {
                        for (const auto& reduction : reductions) {
                            std::string next_value = temps.next();
                            if (reduction.delta >= 0) {
                                rebuilt.push_back(Quad("+", reduction.reduced_temp, std::to_string(reduction.delta), next_value));
                            } else {
                                rebuilt.push_back(Quad("-", reduction.reduced_temp, std::to_string(-reduction.delta), next_value));
                            }
                            rebuilt.push_back(Quad("mov", next_value, "", reduction.reduced_temp));
                        }
                    }

                    rebuilt.push_back(rewritten[index]);
                }

                local.quads = std::move(rebuilt);
                ++stats.loop_preheaders_created;
                ++stats.induction_variables_optimized;
                record_explanation(
                    stats,
                    "Recognized induction variable " + induction_var + " with step " +
                        std::to_string(step) + " in loop " + loop.header_label + ".");
                if (changed != nullptr) {
                    *changed = true;
                }
                transformed = true;
                break;
            }
        }

        append_quads(output.quads, local.quads);
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "strength-reduction pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

IRProgram remove_unreachable_code(const IRProgram& input,
                                  OptimizationStats& stats,
                                  const TraceLogger* trace,
                                  bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        CFG cfg = cfg_builder.build(local);
        int entry_block = !cfg.functions.empty() ? cfg.functions.front().entry_block
                                                 : (!cfg.blocks.empty() ? cfg.blocks.front().id : -1);
        std::vector<int> reachable = reachable_blocks(cfg, entry_block);
        std::unordered_set<int> reachable_set(reachable.begin(), reachable.end());

        std::vector<bool> keep(local.quads.size(), false);
        for (const auto& block : cfg.blocks) {
            if (!reachable_set.count(block.id)) {
                continue;
            }
            for (int quad_index : block.quads) {
                keep[static_cast<size_t>(quad_index)] = true;
            }
        }

        for (size_t index = 0; index < local.quads.size(); ++index) {
            if (keep[index]) {
                output.quads.push_back(local.quads[index]);
            } else {
                ++stats.unreachable_removed;
                if (changed != nullptr) {
                    *changed = true;
                }
            }
        }
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "unreachable-removal pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

IRProgram eliminate_dead_code(const IRProgram& input,
                              OptimizationStats& stats,
                              const TraceLogger* trace,
                              bool* changed) {
    IRProgram output;
    std::vector<FunctionSegment> segments = split_into_functions(input);
    if (segments.empty()) {
        return input;
    }

    CFGBuilder cfg_builder;
    for (const auto& segment : segments) {
        IRProgram local = slice_program(input, segment);
        if (local.quads.empty()) {
            continue;
        }

        CFG cfg = cfg_builder.build(local);
        std::unordered_map<int, std::vector<int>> succs = successor_map(cfg);
        std::unordered_map<int, NameSet> use_sets;
        std::unordered_map<int, NameSet> def_sets;
        std::unordered_map<int, NameSet> live_in;
        std::unordered_map<int, NameSet> live_out;

        for (const auto& block : cfg.blocks) {
            NameSet use_set;
            NameSet def_set;
            for (int quad_index : block.quads) {
                const Quad& quad = local.quads[static_cast<size_t>(quad_index)];
                NameSet uses = used_names(quad);
                for (const auto& name : uses) {
                    if (!def_set.count(name)) {
                        use_set.insert(name);
                    }
                }

                auto def = defined_name(quad);
                if (def.has_value()) {
                    def_set.insert(*def);
                }
            }

            use_sets[block.id] = std::move(use_set);
            def_sets[block.id] = std::move(def_set);
        }

        bool liveness_changed = true;
        while (liveness_changed) {
            liveness_changed = false;
            for (auto block_it = cfg.blocks.rbegin(); block_it != cfg.blocks.rend(); ++block_it) {
                const BasicBlock& block = *block_it;

                NameSet new_live_out;
                auto succ_it = succs.find(block.id);
                if (succ_it != succs.end()) {
                    for (int succ : succ_it->second) {
                        new_live_out.insert(live_in[succ].begin(), live_in[succ].end());
                    }
                }

                NameSet new_live_in = use_sets[block.id];
                for (const auto& name : new_live_out) {
                    if (!def_sets[block.id].count(name)) {
                        new_live_in.insert(name);
                    }
                }

                if (new_live_out != live_out[block.id] || new_live_in != live_in[block.id]) {
                    live_out[block.id] = std::move(new_live_out);
                    live_in[block.id] = std::move(new_live_in);
                    liveness_changed = true;
                }
            }
        }

        if (trace != nullptr && trace->enabled()) {
            for (const auto& block : cfg.blocks) {
                trace->log(
                    "OPT",
                    "liveness B" + std::to_string(block.id) +
                        " in=" + format_name_set(live_in[block.id]) +
                        " out=" + format_name_set(live_out[block.id]));
            }
        }

        std::vector<bool> remove(local.quads.size(), false);
        for (auto block_it = cfg.blocks.rbegin(); block_it != cfg.blocks.rend(); ++block_it) {
            const BasicBlock& block = *block_it;
            NameSet live = live_out[block.id];

            for (auto quad_it = block.quads.rbegin(); quad_it != block.quads.rend(); ++quad_it) {
                int quad_index = *quad_it;
                const Quad& quad = local.quads[static_cast<size_t>(quad_index)];
                NameSet uses = used_names(quad);
                auto def = defined_name(quad);

                if (def.has_value() && is_removable_dead_definition(quad) && !live.count(*def)) {
                    remove[static_cast<size_t>(quad_index)] = true;
                    if (quad.op == "mov" && !is_temporary_name(*def)) {
                        ++stats.dead_stores_removed;
                        record_explanation(stats, "Removed dead store '" + quad.to_string() + "' because " + *def + " is not live afterward.");
                    } else {
                        ++stats.dead_code_removed;
                        record_explanation(stats, "Removed dead temporary computation '" + quad.to_string() + "' because its result is unused.");
                    }
                    if (changed != nullptr) {
                        *changed = true;
                    }
                    continue;
                }

                if (def.has_value()) {
                    live.erase(*def);
                }
                live.insert(uses.begin(), uses.end());
            }
        }

        for (size_t index = 0; index < local.quads.size(); ++index) {
            if (!remove[index]) {
                output.quads.push_back(local.quads[index]);
            }
        }
    }

    if (trace != nullptr && trace->enabled()) {
        trace->log("OPT", "dead-code pass produced " + std::to_string(output.quads.size()) + " quads");
    }

    return output;
}

}  // namespace

void IROptimizer::log(const std::string& message) const {
    if (trace != nullptr) {
        trace->log("OPT", message);
    }
}

IRProgram IROptimizer::optimize(const IRProgram& input, OptimizationStats* stats) const {
    OptimizationStats local_stats;
    OptimizationStats& active_stats = stats != nullptr ? *stats : local_stats;
    active_stats = OptimizationStats{};

    IRProgram current = input;
    for (int iteration = 1; iteration <= 8; ++iteration) {
        bool changed = false;
        log("optimization iteration " + std::to_string(iteration));

        IRProgram cse = eliminate_common_subexpressions(current, active_stats, trace, &changed);
        IRProgram hoisted = hoist_loop_invariants(cse, active_stats, trace, &changed);
        IRProgram first_fold = propagate_and_fold(hoisted, active_stats, trace, &changed);
        IRProgram unswitched = unswitch_loops(first_fold, active_stats, trace, &changed);
        IRProgram peeled = peel_loops(unswitched, active_stats, trace, &changed);
        IRProgram strength_reduced = reduce_strength_and_induction_variables(peeled, active_stats, trace, &changed);
        IRProgram unrolled = unroll_loops(strength_reduced, active_stats, trace, &changed);
        IRProgram propagated = propagate_and_fold(unrolled, active_stats, trace, &changed);
        IRProgram reachable = remove_unreachable_code(propagated, active_stats, trace, &changed);
        IRProgram cleaned = eliminate_dead_code(reachable, active_stats, trace, &changed);

        current = std::move(cleaned);
        if (!changed) {
            break;
        }
    }

    log("constant folds=" + std::to_string(active_stats.constant_folds) +
        ", constant propagations=" + std::to_string(active_stats.constant_propagations) +
        ", common subexpressions eliminated=" + std::to_string(active_stats.common_subexpressions_eliminated) +
        ", dead code removed=" + std::to_string(active_stats.dead_code_removed) +
        ", dead stores removed=" + std::to_string(active_stats.dead_stores_removed) +
        ", unreachable removed=" + std::to_string(active_stats.unreachable_removed) +
        ", loop simplifications=" + std::to_string(active_stats.loop_simplifications) +
        ", loop invariant hoists=" + std::to_string(active_stats.loop_invariant_hoists) +
        ", loop preheaders=" + std::to_string(active_stats.loop_preheaders_created) +
        ", loop peels=" + std::to_string(active_stats.loop_peels) +
        ", loop unrolls=" + std::to_string(active_stats.loop_unrolls) +
        ", loop unswitches=" + std::to_string(active_stats.loop_unswitches) +
        ", strength reductions=" + std::to_string(active_stats.strength_reductions) +
        ", induction variables optimized=" + std::to_string(active_stats.induction_variables_optimized));

    return current;
}
