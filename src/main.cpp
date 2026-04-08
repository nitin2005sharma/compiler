#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cfg.hpp"
#include "ir.hpp"
#include "lexer.hpp"
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

std::string build_phase_summary(const AstStats& ast_stats,
                                size_t token_count,
                                const std::string& astDot,
                                const std::string& astPng,
                                const std::string& annotatedAstDot,
                                const std::string& annotatedAstPng,
                                const std::string& irTxt,
                                size_t quad_count,
                                const std::string& cfgDot,
                                const std::string& cfgPng,
                                const CFG& cfg,
                                bool no_png) {
    size_t entry_edges = 0;
    size_t exit_edges = 0;
    for (const auto& function : cfg.functions) {
        if (function.entry_block >= 0) {
            ++entry_edges;
        }
        exit_edges += function.exit_blocks.size();
    }

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
    summary << "IR: Created " << quad_count << " IR quads and wrote them to " << irTxt << ".\n";
    summary << "CFG: Created a control-flow graph with " << cfg.blocks.size() << " basic blocks, "
            << cfg.edges.size() << " control-flow edges, " << cfg.call_edges.size() << " call edges, and "
            << (entry_edges + exit_edges) << " entry/exit edges in "
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
        } catch (const std::runtime_error& e) {
            std::cerr << "Lex error: " << e.what() << "\n";
            return 1;
        }
        trace.log("LEXER", "completed lexing with " + std::to_string(tokens.size()) + " tokens");

        Parser parser(tokens, trace_enabled ? &trace : nullptr);
        ASTProgram* prog = nullptr;
        trace.log("PARSER", "begin parse");
        try {
            prog = parser.parse_program();
        } catch (const std::runtime_error& e) {
            std::cerr << "Parse error: " << e.what() << "\n";
            return 1;
        }
        trace.log("PARSER", "parse complete");
        AstStats astStats = collect_ast_stats(prog);

        SemanticAnalyzer sem(trace_enabled ? &trace : nullptr);
        trace.log("SEMANTIC", "begin analysis");
        try {
            sem.analyze(prog);
        } catch (const std::runtime_error& e) {
            delete prog;
            std::cerr << "Semantic error: " << e.what() << "\n";
            return 1;
        }
        trace.log("SEMANTIC", "analysis complete");

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
        IRProgram ir = irgen.generate(prog);
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

        CFGBuilder cfgb(trace_enabled ? &trace : nullptr);
        trace.log("CFG", "begin build");
        CFG cfg = cfgb.build(ir);
        std::string cfgDot = outPrefix + "_cfg.dot";
        std::string cfgPng = outPrefix + "_cfg.png";
        write_text_file(cfgDot, cfg.to_dot());
        trace.log("CFG", "wrote " + cfgDot);
        run_dot_if_enabled(no_png, cfgDot, cfgPng, trace);

        std::string summaryTxt = outPrefix + "_summary.txt";
        std::string summary = build_phase_summary(
            astStats,
            tokens.size(),
            astDot,
            astPng,
            annotatedAstDot,
            annotatedAstPng,
            irTxt,
            ir.quads.size(),
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
