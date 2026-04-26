#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"
#include "trace.hpp"

class SemanticAnalyzer {
    struct VarInfo {
        ASTNode* decl = nullptr;
        int scope_depth = -1;
        std::string kind;
        std::string type_name;
    };

    struct Scope {
        std::unordered_map<std::string, VarInfo> vars;
    };

    struct FunctionInfo {
        size_t arity = 0;
        FuncDecl* decl = nullptr;
        std::string signature;
    };

    std::vector<Scope> scopes;
    std::unordered_map<std::string, FunctionInfo> functions;
    std::vector<std::string> report_lines;
    TraceLogger* trace;

    void log(const std::string& message) const;
    void enter();
    void leave();
    const VarInfo* resolve_var(const std::string& n) const;
    void declare(const std::string& name, ASTNode* decl, const std::string& kind);
    std::string make_function_signature(const FuncDecl* func) const;

public:
    explicit SemanticAnalyzer(TraceLogger* trace_logger = nullptr)
        : trace(trace_logger) {}

    void analyze(ASTProgram* prog);
    std::string symbol_report() const;

private:
    void analyze_node(ASTNode* n);
    void analyze_func(FuncDecl* f);
    void analyze_stmt(ASTStmt* s);
    void analyze_expr(ASTExpr* e);
};

#endif
