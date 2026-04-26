#include <array>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cfg.hpp"
#include "diagnostics.hpp"
#include "ir.hpp"
#include "lexer.hpp"
#include "optimizer.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include "trace.hpp"

namespace {

struct AstStats {
    size_t top_level_decls = 0;
    size_t functions = 0;
    size_t var_decls = 0;
    size_t statements = 0;
    size_t call_exprs = 0;
};

void print_usage() {
    std::cout << "Usage: compiler [--trace] [--no-png] <source> [out_prefix]\n";
}

std::string quote_path(const std::string& path) {
    return "\"" + path + "\"";
}

std::string read_env_var(const char* name) {
#ifdef _WIN32
    char* buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return "";
    }
    std::string value(buffer);
    free(buffer);
    return value;
#else
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
#endif
}

std::string find_dot_executable() {
    std::string env_dot = read_env_var("DOT_PATH");
    if (!env_dot.empty() && std::filesystem::exists(env_dot)) {
        return env_dot;
    }

    const std::array<std::string, 2> common_paths = {
        "C:\\Program Files\\Graphviz\\bin\\dot.exe",
        "C:\\Program Files (x86)\\Graphviz\\bin\\dot.exe"
    };

    for (const auto& candidate : common_paths) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return "dot";
}

void write_text_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Cannot write file: " + path);
    }
    out << content;
}

void run_dot_if_enabled(bool no_png, const std::string& dot_path, const std::string& png_path, const TraceLogger& trace) {
    if (no_png) {
        trace.log("DRIVER", "skipping PNG generation for " + dot_path);
        return;
    }

    std::string dot_exe = find_dot_executable();
    std::string invocation = quote_path(dot_exe) + " -Tpng " + quote_path(dot_path) + " -o " + quote_path(png_path);
#ifdef _WIN32
    std::string cmd = "cmd /c " + quote_path(invocation);
#else
    std::string cmd = invocation;
#endif
    trace.log("DRIVER", "running " + cmd);
    int rc = system(cmd.c_str());
    if (rc != 0) {
        std::cerr << "Warning: Graphviz 'dot' command failed for " << dot_path << "\n";
    }
}

void accumulate_expr_stats(ASTExpr* expr, AstStats& stats) {
    if (expr == nullptr) {
        return;
    }

    if (auto* binary = dynamic_cast<BinaryExpr*>(expr)) {
        accumulate_expr_stats(binary->lhs, stats);
        accumulate_expr_stats(binary->rhs, stats);
        return;
    }

    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        ++stats.call_exprs;
        for (auto* arg : call->args) {
            accumulate_expr_stats(arg, stats);
        }
    }
}

void accumulate_stmt_stats(ASTStmt* stmt, AstStats& stats) {
    if (stmt == nullptr) {
        return;
    }

    ++stats.statements;

    if (auto* var = dynamic_cast<VarDecl*>(stmt)) {
        ++stats.var_decls;
        (void)var;
        return;
    }

    if (auto* block = dynamic_cast<Block*>(stmt)) {
        for (auto* nested : block->stmts) {
            accumulate_stmt_stats(nested, stats);
        }
        return;
    }

    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt)) {
        accumulate_expr_stats(if_stmt->cond, stats);
        accumulate_stmt_stats(if_stmt->then_s, stats);
        accumulate_stmt_stats(if_stmt->else_s, stats);
        return;
    }

    if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt)) {
        accumulate_expr_stats(while_stmt->cond, stats);
        accumulate_stmt_stats(while_stmt->body, stats);
        return;
    }

    if (auto* ret = dynamic_cast<ReturnStmt*>(stmt)) {
        accumulate_expr_stats(ret->expr, stats);
        return;
    }

    if (auto* assign = dynamic_cast<AssignStmt*>(stmt)) {
        accumulate_expr_stats(assign->expr, stats);
        return;
    }

    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
        accumulate_expr_stats(expr_stmt->expr, stats);
    }
}

AstStats collect_ast_stats(ASTProgram* prog) {
    AstStats stats;
    stats.top_level_decls = prog != nullptr ? prog->decls.size() : 0;

    if (prog == nullptr) {
        return stats;
    }

    for (auto* decl : prog->decls) {
        if (auto* var = dynamic_cast<VarDecl*>(decl)) {
            ++stats.var_decls;
            (void)var;
            continue;
        }

        auto* func = dynamic_cast<FuncDecl*>(decl);
        if (func == nullptr) {
            continue;
        }

        ++stats.functions;
        for (auto* stmt : func->body) {
            accumulate_stmt_stats(stmt, stats);
        }
    }

    return stats;
}

std::string graph_outputs(const std::string& dot_path, const std::string& png_path, bool no_png) {
    if (no_png) {
        return dot_path + " (PNG skipped)";
    }
    return dot_path + " and " + png_path;
}

size_t cfg_entry_exit_edges(const CFG& cfg) {
    size_t total = 0;
    for (const auto& function : cfg.functions) {
        if (function.entry_block >= 0) {
            ++total;
        }
        total += function.exit_blocks.size();
    }
    return total;
}

bool report_is_integer_literal(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    size_t start = value[0] == '-' ? 1 : 0;
    if (start == value.size()) {
        return false;
    }
    return std::all_of(value.begin() + static_cast<std::ptrdiff_t>(start), value.end(), [](char ch) {
        return std::isdigit(static_cast<unsigned char>(ch));
    });
}

bool report_is_identifier(const std::string& value) {
    if (value.empty() || report_is_integer_literal(value)) {
        return false;
    }
    if (!(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
    });
}

bool report_is_binary_operator(const std::string& op) {
    static const std::unordered_set<std::string> ops = {
        "+", "-", "*", "/", "<", "<=", ">", ">=", "==", "!="
    };
    return ops.count(op) > 0;
}

std::optional<std::string> report_defined_name(const Quad& quad) {
    if ((quad.op == "mov" || quad.op == "arg" || quad.op == "call" || report_is_binary_operator(quad.op)) &&
        !quad.res.empty()) {
        return quad.res;
    }
    return std::nullopt;
}

std::unordered_set<std::string> report_used_names(const Quad& quad) {
    std::unordered_set<std::string> names;
    auto add = [&](const std::string& value) {
        if (report_is_identifier(value)) {
            names.insert(value);
        }
    };

    if (quad.op == "ifFalse" || quad.op == "ret" || quad.op == "mov" || quad.op == "param") {
        add(quad.a);
    } else if (report_is_binary_operator(quad.op)) {
        add(quad.a);
        add(quad.b);
    }
    return names;
}

std::string format_name_set(const std::unordered_set<std::string>& names) {
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

std::unordered_map<int, std::vector<int>> cfg_successors(const CFG& cfg) {
    std::unordered_map<int, std::vector<int>> succs;
    for (const auto& edge : cfg.edges) {
        succs[edge.from].push_back(edge.to);
    }
    return succs;
}

std::unordered_set<int> cfg_reachable_blocks(const CFG& cfg) {
    std::unordered_set<int> reachable;
    std::queue<int> work;
    auto succs = cfg_successors(cfg);

    for (const auto& function : cfg.functions) {
        if (function.entry_block >= 0 && reachable.insert(function.entry_block).second) {
            work.push(function.entry_block);
        }
    }
    if (cfg.functions.empty() && !cfg.blocks.empty()) {
        reachable.insert(cfg.blocks.front().id);
        work.push(cfg.blocks.front().id);
    }

    while (!work.empty()) {
        int block_id = work.front();
        work.pop();
        for (int succ : succs[block_id]) {
            if (reachable.insert(succ).second) {
                work.push(succ);
            }
        }
    }
    return reachable;
}

std::string build_liveness_report(const IRProgram& ir, const CFG& cfg) {
    auto succs = cfg_successors(cfg);
    std::unordered_map<int, std::unordered_set<std::string>> use_sets;
    std::unordered_map<int, std::unordered_set<std::string>> def_sets;
    std::unordered_map<int, std::unordered_set<std::string>> live_in;
    std::unordered_map<int, std::unordered_set<std::string>> live_out;

    for (const auto& block : cfg.blocks) {
        std::unordered_set<std::string> use_set;
        std::unordered_set<std::string> def_set;
        for (int quad_index : block.quads) {
            const Quad& quad = ir.quads[static_cast<size_t>(quad_index)];
            for (const auto& name : report_used_names(quad)) {
                if (!def_set.count(name)) {
                    use_set.insert(name);
                }
            }
            auto def = report_defined_name(quad);
            if (def.has_value()) {
                def_set.insert(*def);
            }
        }
        use_sets[block.id] = std::move(use_set);
        def_sets[block.id] = std::move(def_set);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto block_it = cfg.blocks.rbegin(); block_it != cfg.blocks.rend(); ++block_it) {
            const BasicBlock& block = *block_it;
            std::unordered_set<std::string> next_live_out;
            for (int succ : succs[block.id]) {
                next_live_out.insert(live_in[succ].begin(), live_in[succ].end());
            }

            std::unordered_set<std::string> next_live_in = use_sets[block.id];
            for (const auto& name : next_live_out) {
                if (!def_sets[block.id].count(name)) {
                    next_live_in.insert(name);
                }
            }

            if (next_live_in != live_in[block.id] || next_live_out != live_out[block.id]) {
                live_in[block.id] = std::move(next_live_in);
                live_out[block.id] = std::move(next_live_out);
                changed = true;
            }
        }
    }

    std::ostringstream os;
    os << "Liveness Analysis\n";
    os << "=================\n";
    for (const auto& block : cfg.blocks) {
        os << "B" << block.id << " (" << (block.function_name.empty() ? "global" : block.function_name) << ")\n";
        os << "  use:      " << format_name_set(use_sets[block.id]) << "\n";
        os << "  def:      " << format_name_set(def_sets[block.id]) << "\n";
        os << "  live-in:  " << format_name_set(live_in[block.id]) << "\n";
        os << "  live-out: " << format_name_set(live_out[block.id]) << "\n";
        os << "\n";
    }
    return os.str();
}

std::string versioned_name(const std::string& value, const std::unordered_map<std::string, int>& versions) {
    if (!report_is_identifier(value)) {
        return value;
    }
    auto it = versions.find(value);
    if (it == versions.end()) {
        return value + ".0";
    }
    return value + "." + std::to_string(it->second);
}

std::string format_ssa_quad(const Quad& quad, std::unordered_map<std::string, int>& versions) {
    if (quad.op == "label") {
        return quad.res + ":";
    }
    if (quad.op == "goto") {
        return "goto " + quad.res;
    }
    if (quad.op == "ifFalse") {
        return "ifFalse " + versioned_name(quad.a, versions) + " goto " + quad.res;
    }
    if (quad.op == "ret") {
        return "ret " + versioned_name(quad.a, versions);
    }
    if (quad.op == "param") {
        return "param " + versioned_name(quad.a, versions);
    }

    auto make_def = [&](const std::string& name) {
        int next_version = ++versions[name];
        return name + "." + std::to_string(next_version);
    };

    if (quad.op == "arg") {
        return make_def(quad.res) + " = arg " + quad.a;
    }
    if (quad.op == "call") {
        return make_def(quad.res) + " = call " + quad.a + ", " + quad.b;
    }
    if (quad.op == "mov") {
        return make_def(quad.res) + " = " + versioned_name(quad.a, versions);
    }
    if (report_is_binary_operator(quad.op)) {
        return make_def(quad.res) + " = " + versioned_name(quad.a, versions) + " " + quad.op + " " + versioned_name(quad.b, versions);
    }
    return quad.to_string();
}

std::string build_ssa_report(const IRProgram& ir) {
    std::ostringstream os;
    std::unordered_map<std::string, int> versions;
    os << "SSA-style IR\n";
    os << "============\n";
    os << "This teaching view versions assignments so data flow is easier to inspect. It does not insert phi nodes yet.\n\n";
    for (size_t index = 0; index < ir.quads.size(); ++index) {
        os << index << ": " << format_ssa_quad(ir.quads[index], versions) << "\n";
    }
    return os.str();
}

std::string build_optimization_report(const OptimizationStats& stats) {
    std::ostringstream os;
    os << "Optimization Explanations\n";
    os << "=========================\n";
    os << "constant folds: " << stats.constant_folds << "\n";
    os << "constant propagations: " << stats.constant_propagations << "\n";
    os << "common subexpressions eliminated: " << stats.common_subexpressions_eliminated << "\n";
    os << "dead code removed: " << stats.dead_code_removed << "\n";
    os << "dead stores removed: " << stats.dead_stores_removed << "\n";
    os << "unreachable code removed: " << stats.unreachable_removed << "\n";
    os << "loop invariant hoists: " << stats.loop_invariant_hoists << "\n";
    os << "loop preheaders: " << stats.loop_preheaders_created << "\n";
    os << "loop peels: " << stats.loop_peels << "\n";
    os << "loop unrolls: " << stats.loop_unrolls << "\n";
    os << "loop unswitches: " << stats.loop_unswitches << "\n";
    os << "strength reductions: " << stats.strength_reductions << "\n";
    os << "induction variables optimized: " << stats.induction_variables_optimized << "\n\n";

    if (stats.explanations.empty()) {
        os << "No individual optimization explanations were recorded for this run.\n";
        return os.str();
    }

    for (size_t index = 0; index < stats.explanations.size(); ++index) {
        os << index + 1 << ". " << stats.explanations[index] << "\n";
    }
    return os.str();
}

std::string escape_report_dot_label(const std::string& value) {
    std::string escaped;
    for (char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string build_cfg_analysis_dot(const CFG& cfg) {
    std::unordered_set<int> reachable = cfg_reachable_blocks(cfg);
    std::unordered_set<int> loop_headers;
    std::unordered_set<int> latch_blocks;
    std::unordered_set<int> preheaders;
    auto succs = cfg_successors(cfg);

    for (const auto& edge : cfg.edges) {
        if (edge.label == "goto" && edge.to <= edge.from) {
            loop_headers.insert(edge.to);
            latch_blocks.insert(edge.from);
        }
    }
    for (const auto& edge : cfg.edges) {
        if (loop_headers.count(edge.to) && !latch_blocks.count(edge.from) && succs[edge.from].size() == 1) {
            preheaders.insert(edge.from);
        }
    }

    std::ostringstream os;
    os << "digraph CFGAnalysis{graph[compound=true,ranksep=0.7,nodesep=0.5];\n";
    os << "node[shape=box,fontname=\"Consolas\",style=\"rounded,filled\"];\n";
    os << "edge[fontname=\"Consolas\"];\n";

    for (const auto& block : cfg.blocks) {
        std::vector<std::string> tags;
        std::string fill = "#eaf4ff";
        if (!reachable.count(block.id)) {
            tags.push_back("unreachable");
            fill = "#d9d9d9";
        }
        if (preheaders.count(block.id)) {
            tags.push_back("preheader");
            fill = "#d8f3dc";
        }
        if (loop_headers.count(block.id)) {
            tags.push_back("loop header");
            fill = "#fff3bf";
        }
        if (latch_blocks.count(block.id)) {
            tags.push_back("backedge latch");
            if (!loop_headers.count(block.id)) {
                fill = "#ffe5d9";
            }
        }

        os << "B" << block.id << "[fillcolor=\"" << fill << "\",label=\"B" << block.id;
        if (!tags.empty()) {
            os << " [";
            for (size_t index = 0; index < tags.size(); ++index) {
                if (index > 0) {
                    os << ", ";
                }
                os << tags[index];
            }
            os << "]";
        }
        os << "\\l";
        for (const auto& instruction : block.instructions) {
            os << escape_report_dot_label(instruction) << "\\l";
        }
        os << "\"];\n";
    }

    for (const auto& edge : cfg.edges) {
        bool backedge = edge.label == "goto" && edge.to <= edge.from;
        os << "B" << edge.from << " -> B" << edge.to << "[label=\""
           << escape_report_dot_label(backedge ? "backedge" : edge.label) << "\"";
        if (backedge) {
            os << ",color=\"#c1121f\",penwidth=2.2";
        }
        os << "];\n";
    }

    os << "}\n";
    return os.str();
}

std::string build_cfg_analysis_report(const CFG& cfg) {
    std::unordered_set<int> reachable = cfg_reachable_blocks(cfg);
    std::unordered_set<int> loop_headers;
    std::unordered_set<int> latch_blocks;
    std::unordered_set<int> preheaders;
    auto succs = cfg_successors(cfg);

    for (const auto& edge : cfg.edges) {
        if (edge.label == "goto" && edge.to <= edge.from) {
            loop_headers.insert(edge.to);
            latch_blocks.insert(edge.from);
        }
    }
    for (const auto& edge : cfg.edges) {
        if (loop_headers.count(edge.to) && !latch_blocks.count(edge.from) && succs[edge.from].size() == 1) {
            preheaders.insert(edge.from);
        }
    }

    std::ostringstream os;
    os << "CFG / Dominator-Oriented Metadata\n";
    os << "=================================\n";
    os << "Loop headers: " << loop_headers.size() << "\n";
    os << "Backedge latch blocks: " << latch_blocks.size() << "\n";
    os << "Preheaders: " << preheaders.size() << "\n";
    os << "Unreachable blocks: " << (cfg.blocks.size() - reachable.size()) << "\n\n";
    for (const auto& edge : cfg.edges) {
        if (edge.label == "goto" && edge.to <= edge.from) {
            os << "- backedge: B" << edge.from << " -> B" << edge.to << "\n";
        }
    }
    for (int block_id : preheaders) {
        os << "- preheader: B" << block_id << "\n";
    }
    for (const auto& block : cfg.blocks) {
        if (!reachable.count(block.id)) {
            os << "- unreachable: B" << block.id << "\n";
        }
    }
    return os.str();
}

std::string build_phase_summary(const AstStats& ast_stats,
                                size_t token_count,
                                const std::string& astDot,
                                const std::string& astPng,
                                const std::string& annotatedAstDot,
                                const std::string& annotatedAstPng,
                                const std::string& preOptIrTxt,
                                size_t preOptQuadCount,
                                const std::string& irTxt,
                                size_t quad_count,
                                const OptimizationStats& opt_stats,
                                const std::string& preOptCfgDot,
                                const std::string& preOptCfgPng,
                                const CFG& preOptCfg,
                                const std::string& cfgDot,
                                const std::string& cfgPng,
                                const CFG& cfg,
                                bool no_png) {
    std::ostringstream summary;
    summary << "LEXER: Created a token stream with " << token_count << " tokens including the END marker.\n";
    summary << "PARSER: Created an AST with " << ast_stats.top_level_decls << " top-level declarations, "
            << ast_stats.functions << " functions, " << ast_stats.var_decls << " variable declarations, and "
            << ast_stats.call_exprs << " call expressions.\n";
    summary << "SEMANTIC: Created symbol and function tables, resolved declaration/use links, recorded scope depth and node types, and validated function-call arity.\n";
    summary << "AST: Created the AST graph files " << graph_outputs(astDot, astPng, no_png) << ".\n";
    summary << "ANNOTATED AST: Created the annotated AST graph files "
            << graph_outputs(annotatedAstDot, annotatedAstPng, no_png)
            << " with semantic labels such as scope, type, signature, and declaration links.\n";
    summary << "PRE-OPT IR: Created " << preOptQuadCount << " unoptimized IR quads and wrote them to "
            << preOptIrTxt << ".\n";
    summary << "IR: Created " << quad_count << " optimized IR quads and wrote them to " << irTxt << ".\n";
    summary << "PRE-OPT CFG: Created a control-flow graph from the unoptimized IR with "
            << preOptCfg.blocks.size() << " basic blocks, "
            << preOptCfg.edges.size() << " control-flow edges, "
            << preOptCfg.call_edges.size() << " call edges, and "
            << cfg_entry_exit_edges(preOptCfg) << " entry/exit edges in "
            << graph_outputs(preOptCfgDot, preOptCfgPng, no_png) << ".\n";
    summary << "OPT: Applied " << opt_stats.constant_folds << " constant folds, "
            << opt_stats.constant_propagations << " constant propagations, "
            << opt_stats.common_subexpressions_eliminated << " common-subexpression eliminations, "
            << opt_stats.dead_code_removed << " dead-code removals, "
            << opt_stats.dead_stores_removed << " dead-store removals, "
            << opt_stats.unreachable_removed << " unreachable-code removals, "
            << opt_stats.loop_simplifications << " loop simplifications, "
            << opt_stats.loop_invariant_hoists << " loop-invariant hoists, "
            << opt_stats.loop_preheaders_created << " loop preheaders, "
            << opt_stats.loop_peels << " loop peels, "
            << opt_stats.loop_unrolls << " loop unrolls, and "
            << opt_stats.loop_unswitches << " loop unswitches, "
            << opt_stats.strength_reductions << " strength reductions, and "
            << opt_stats.induction_variables_optimized << " induction variables optimized.\n";
    summary << "CFG: Created an optimized control-flow graph with " << cfg.blocks.size() << " basic blocks, "
            << cfg.edges.size() << " control-flow edges, " << cfg.call_edges.size() << " call edges, and "
            << cfg_entry_exit_edges(cfg) << " entry/exit edges in "
            << graph_outputs(cfgDot, cfgPng, no_png) << ".\n";
    return summary.str();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        bool trace_enabled = false;
        bool no_png = false;
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--trace") {
                trace_enabled = true;
            } else if (arg == "--no-png") {
                no_png = true;
            } else if (arg == "--help" || arg == "-h") {
                print_usage();
                return 0;
            } else {
                positional.push_back(arg);
            }
        }

        if (positional.empty() || positional.size() > 2) {
            print_usage();
            return 1;
        }

        TraceLogger trace(trace_enabled, &std::cout);
        trace.log("DRIVER", "main entered");

        std::string srcPath = positional[0];
        std::string outPrefix = positional.size() >= 2 ? positional[1] : "out";
        trace.log("DRIVER", "source=" + srcPath + ", outPrefix=" + outPrefix);

        std::ifstream ifs(srcPath);
        if (!ifs) {
            std::cerr << "Cannot open source\n";
            return 1;
        }

        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string code = ss.str();
        trace.log("DRIVER", "loaded source bytes=" + std::to_string(code.size()));

        Lexer lexer(code);
        std::vector<Token> tokens;
        trace.log("LEXER", "begin lexing");
        try {
            while (true) {
                Token t = lexer.next();
                tokens.push_back(t);
                trace.log("LEXER", token_to_string(t));
                if (t.type == TokenType::END) {
                    break;
                }
            }
        } catch (const CompileError& e) {
            std::cerr << format_compile_error(e, code) << "\n";
            return 1;
        }
        trace.log("LEXER", "completed lexing with " + std::to_string(tokens.size()) + " tokens");

        Parser parser(tokens, trace_enabled ? &trace : nullptr);
        ASTProgram* prog = nullptr;
        trace.log("PARSER", "begin parse");
        try {
            prog = parser.parse_program();
        } catch (const CompileError& e) {
            std::cerr << format_compile_error(e, code) << "\n";
            return 1;
        }
        trace.log("PARSER", "parse complete");
        AstStats astStats = collect_ast_stats(prog);

        SemanticAnalyzer sem(trace_enabled ? &trace : nullptr);
        trace.log("SEMANTIC", "begin analysis");
        try {
            sem.analyze(prog);
        } catch (const CompileError& e) {
            delete prog;
            std::cerr << format_compile_error(e, code) << "\n";
            return 1;
        }
        trace.log("SEMANTIC", "analysis complete");

        std::string symbolsTxt = outPrefix + "_symbols.txt";
        write_text_file(symbolsTxt, sem.symbol_report());
        trace.log("SEMANTIC", "wrote " + symbolsTxt);

        std::string astDot = outPrefix + "_ast.dot";
        std::string astPng = outPrefix + "_ast.png";
        trace.log("AST", "writing graph " + astDot);
        write_text_file(astDot, prog->to_dot());
        run_dot_if_enabled(no_png, astDot, astPng, trace);

        std::string annotatedAstDot = outPrefix + "_annotated_ast.dot";
        std::string annotatedAstPng = outPrefix + "_annotated_ast.png";
        trace.log("AST", "writing annotated graph " + annotatedAstDot);
        write_text_file(annotatedAstDot, prog->to_annotated_dot());
        run_dot_if_enabled(no_png, annotatedAstDot, annotatedAstPng, trace);

        IRGenerator irgen(trace_enabled ? &trace : nullptr);
        trace.log("IR", "begin generation");
        IRProgram rawIr = irgen.generate(prog);
        std::string preOptIrTxt = outPrefix + "_pre_opt_ir.txt";
        {
            std::ofstream preOptIrFile(preOptIrTxt);
            if (!preOptIrFile) {
                delete prog;
                std::cerr << "Cannot write pre-optimization IR output\n";
                return 1;
            }
            for (size_t i = 0; i < rawIr.quads.size(); ++i) {
                preOptIrFile << i << ": " << rawIr.quads[i].to_string() << "\n";
            }
        }
        trace.log("IR", "wrote " + preOptIrTxt);
        CFGBuilder cfgb(trace_enabled ? &trace : nullptr);
        trace.log("CFG", "begin build for pre-optimization CFG");
        CFG preOptCfg = cfgb.build(rawIr);
        std::string preOptCfgDot = outPrefix + "_pre_opt_cfg.dot";
        std::string preOptCfgPng = outPrefix + "_pre_opt_cfg.png";
        write_text_file(preOptCfgDot, preOptCfg.to_dot());
        trace.log("CFG", "wrote " + preOptCfgDot);
        run_dot_if_enabled(no_png, preOptCfgDot, preOptCfgPng, trace);

        IROptimizer optimizer(trace_enabled ? &trace : nullptr);
        OptimizationStats optStats;
        trace.log("OPT", "begin optimization");
        IRProgram ir = optimizer.optimize(rawIr, &optStats);
        trace.log("OPT", "optimization complete");
        std::string irTxt = outPrefix + "_ir.txt";
        {
            std::ofstream irf(irTxt);
            if (!irf) {
                delete prog;
                std::cerr << "Cannot write IR output\n";
                return 1;
            }
            for (size_t i = 0; i < ir.quads.size(); ++i) {
                irf << i << ": " << ir.quads[i].to_string() << "\n";
            }
        }
        for (const auto& q : ir.quads) {
            std::cout << q.to_string() << "\n";
        }
        trace.log("IR", "wrote " + irTxt);

        trace.log("CFG", "begin build for optimized CFG");
        CFG cfg = cfgb.build(ir);
        std::string cfgDot = outPrefix + "_cfg.dot";
        std::string cfgPng = outPrefix + "_cfg.png";
        write_text_file(cfgDot, cfg.to_dot());
        trace.log("CFG", "wrote " + cfgDot);
        run_dot_if_enabled(no_png, cfgDot, cfgPng, trace);

        std::string optReportTxt = outPrefix + "_opt_report.txt";
        write_text_file(optReportTxt, build_optimization_report(optStats));
        trace.log("OPT", "wrote " + optReportTxt);

        std::string ssaIrTxt = outPrefix + "_ssa_ir.txt";
        write_text_file(ssaIrTxt, build_ssa_report(ir));
        trace.log("IR", "wrote " + ssaIrTxt);

        std::string livenessTxt = outPrefix + "_liveness.txt";
        write_text_file(livenessTxt, build_liveness_report(ir, cfg));
        trace.log("OPT", "wrote " + livenessTxt);

        std::string cfgAnalysisTxt = outPrefix + "_cfg_analysis.txt";
        write_text_file(cfgAnalysisTxt, build_cfg_analysis_report(cfg));
        trace.log("CFG", "wrote " + cfgAnalysisTxt);

        std::string cfgAnalysisDot = outPrefix + "_cfg_analysis.dot";
        std::string cfgAnalysisPng = outPrefix + "_cfg_analysis.png";
        write_text_file(cfgAnalysisDot, build_cfg_analysis_dot(cfg));
        trace.log("CFG", "wrote " + cfgAnalysisDot);
        run_dot_if_enabled(no_png, cfgAnalysisDot, cfgAnalysisPng, trace);

        std::string summaryTxt = outPrefix + "_summary.txt";
        std::string summary = build_phase_summary(
            astStats,
            tokens.size(),
            astDot,
            astPng,
            annotatedAstDot,
            annotatedAstPng,
            preOptIrTxt,
            rawIr.quads.size(),
            irTxt,
            ir.quads.size(),
            optStats,
            preOptCfgDot,
            preOptCfgPng,
            preOptCfg,
            cfgDot,
            cfgPng,
            cfg,
            no_png
        );
        write_text_file(summaryTxt, summary);
        if (trace_enabled) {
            std::istringstream summaryStream(summary);
            std::string line;
            while (std::getline(summaryStream, line)) {
                if (!line.empty()) {
                    trace.log("SUMMARY", line);
                }
            }
            trace.log("SUMMARY", "Wrote summary file " + summaryTxt);
        }

        delete prog;
        trace.log("DRIVER", "completed successfully");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unhandled unknown exception\n";
        return 1;
    }
}
