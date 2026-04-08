#include "semantic.hpp"

#include <sstream>
#include <stdexcept>

void SemanticAnalyzer::log(const std::string& message) const {
    if (trace != nullptr) {
        trace->log("SEMANTIC", message);
    }
}

void SemanticAnalyzer::enter() {
    scopes.emplace_back();
    log("enter scope depth " + std::to_string(scopes.size() - 1));
}

void SemanticAnalyzer::leave() {
    log("leave scope depth " + std::to_string(scopes.size() - 1));
    scopes.pop_back();
}

const SemanticAnalyzer::VarInfo* SemanticAnalyzer::resolve_var(const std::string& n) const {
    for (int i = static_cast<int>(scopes.size()) - 1; i >= 0; --i) {
        auto it = scopes[static_cast<size_t>(i)].vars.find(n);
        if (it != scopes[static_cast<size_t>(i)].vars.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

void SemanticAnalyzer::declare(const std::string& name, ASTNode* decl, const std::string& kind) {
    if (scopes.empty()) {
        enter();
    }

    if (functions.count(name)) {
        throw std::runtime_error("Variable name conflicts with function: " + name);
    }

    auto& current = scopes.back().vars;
    if (current.count(name)) {
        throw std::runtime_error("Duplicate declaration: " + name);
    }

    VarInfo info;
    info.decl = decl;
    info.scope_depth = static_cast<int>(scopes.size()) - 1;
    info.kind = kind;
    info.type_name = "int";
    current[name] = info;

    decl->semantic.symbol_kind = kind;
    decl->semantic.type_name = info.type_name;
    decl->semantic.scope_depth = info.scope_depth;
    decl->semantic.declaration = decl;

    log("declared " + kind + " " + name + " at depth " + std::to_string(info.scope_depth));
}

std::string SemanticAnalyzer::make_function_signature(const FuncDecl* func) const {
    std::ostringstream signature;
    signature << "int " << func->name << "(";
    for (size_t index = 0; index < func->params.size(); ++index) {
        if (index != 0) {
            signature << ", ";
        }
        signature << "int";
    }
    signature << ")";
    return signature.str();
}

void SemanticAnalyzer::analyze(ASTProgram* prog) {
    scopes.clear();
    functions.clear();
    enter();

    prog->semantic.symbol_kind = "program";

    bool has_main = false;
    log("begin semantic analysis");

    for (auto* d : prog->decls) {
        if (auto* v = dynamic_cast<VarDecl*>(d)) {
            declare(v->name, v, "global");
            continue;
        }

        auto* f = dynamic_cast<FuncDecl*>(d);
        if (f == nullptr) {
            continue;
        }

        if (!scopes.empty() && scopes.front().vars.count(f->name)) {
            throw std::runtime_error("Function name conflicts with variable: " + f->name);
        }
        if (functions.count(f->name)) {
            throw std::runtime_error("Duplicate function: " + f->name);
        }

        FunctionInfo info;
        info.arity = f->params.size();
        info.decl = f;
        info.signature = make_function_signature(f);
        functions[f->name] = info;

        f->semantic.symbol_kind = "function";
        f->semantic.type_name = "int";
        f->semantic.signature = info.signature;
        f->semantic.scope_depth = 0;
        f->semantic.declaration = f;

        log("registered function " + f->name + " with signature " + info.signature);
        if (f->name == "main") {
            has_main = true;
        }
    }

    if (!has_main) {
        throw std::runtime_error("Missing main function");
    }

    for (auto* d : prog->decls) {
        analyze_node(d);
    }

    leave();
    log("semantic analysis complete");
}

void SemanticAnalyzer::analyze_node(ASTNode* n) {
    if (auto* f = dynamic_cast<FuncDecl*>(n)) {
        analyze_func(f);
    }
}

void SemanticAnalyzer::analyze_func(FuncDecl* f) {
    log("analyze function " + f->name);
    enter();
    for (auto* param : f->params) {
        param->semantic.signature = "int parameter";
        declare(param->name, param, "parameter");
    }
    for (auto* s : f->body) {
        analyze_stmt(s);
    }
    leave();
}

void SemanticAnalyzer::analyze_stmt(ASTStmt* s) {
    if (auto* vd = dynamic_cast<VarDecl*>(s)) {
        declare(vd->name, vd, "variable");
        return;
    }

    if (auto* b = dynamic_cast<Block*>(s)) {
        b->semantic.symbol_kind = "block";
        b->semantic.scope_depth = static_cast<int>(scopes.size());
        enter();
        for (auto* ss : b->stmts) {
            analyze_stmt(ss);
        }
        leave();
        return;
    }

    if (auto* is = dynamic_cast<IfStmt*>(s)) {
        is->semantic.symbol_kind = "branch";
        is->semantic.scope_depth = static_cast<int>(scopes.size()) - 1;
        analyze_expr(is->cond);
        analyze_stmt(is->then_s);
        if (is->else_s) {
            analyze_stmt(is->else_s);
        }
        return;
    }

    if (auto* ws = dynamic_cast<WhileStmt*>(s)) {
        ws->semantic.symbol_kind = "loop";
        ws->semantic.scope_depth = static_cast<int>(scopes.size()) - 1;
        analyze_expr(ws->cond);
        analyze_stmt(ws->body);
        return;
    }

    if (auto* rs = dynamic_cast<ReturnStmt*>(s)) {
        rs->semantic.symbol_kind = "return";
        rs->semantic.type_name = "int";
        rs->semantic.scope_depth = static_cast<int>(scopes.size()) - 1;
        if (rs->expr) {
            analyze_expr(rs->expr);
        }
        return;
    }

    if (auto* as = dynamic_cast<AssignStmt*>(s)) {
        const VarInfo* target = resolve_var(as->name);
        if (target == nullptr) {
            throw std::runtime_error("Undeclared variable: " + as->name);
        }
        as->semantic.symbol_kind = "write";
        as->semantic.type_name = target->type_name;
        as->semantic.scope_depth = target->scope_depth;
        as->semantic.declaration = target->decl;
        log("validated assignment target " + as->name + " at depth " + std::to_string(target->scope_depth));
        analyze_expr(as->expr);
        return;
    }

    if (auto* es = dynamic_cast<ExprStmt*>(s)) {
        es->semantic.symbol_kind = "expr_stmt";
        es->semantic.scope_depth = static_cast<int>(scopes.size()) - 1;
        analyze_expr(es->expr);
        return;
    }

    throw std::runtime_error("Unknown statement in semantic analysis");
}

void SemanticAnalyzer::analyze_expr(ASTExpr* e) {
    if (auto* literal = dynamic_cast<IntLiteral*>(e)) {
        literal->semantic.symbol_kind = "literal";
        literal->semantic.type_name = "int";
        return;
    }

    if (auto* id = dynamic_cast<Ident*>(e)) {
        const VarInfo* resolved = resolve_var(id->name);
        if (resolved == nullptr) {
            throw std::runtime_error("Undeclared variable: " + id->name);
        }
        id->semantic.symbol_kind = "use";
        id->semantic.type_name = resolved->type_name;
        id->semantic.scope_depth = resolved->scope_depth;
        id->semantic.declaration = resolved->decl;
        log("resolved identifier " + id->name + " to depth " + std::to_string(resolved->scope_depth));
        return;
    }

    if (auto* be = dynamic_cast<BinaryExpr*>(e)) {
        analyze_expr(be->lhs);
        analyze_expr(be->rhs);
        be->semantic.symbol_kind = "binary_expr";
        be->semantic.type_name = "int";
        return;
    }

    if (auto* call = dynamic_cast<CallExpr*>(e)) {
        auto it = functions.find(call->callee);
        if (it == functions.end()) {
            throw std::runtime_error("Undefined function: " + call->callee);
        }
        if (it->second.arity != call->args.size()) {
            std::ostringstream os;
            os << "Function " << call->callee << " expects " << it->second.arity
               << " arguments but got " << call->args.size();
            throw std::runtime_error(os.str());
        }

        call->semantic.symbol_kind = "call";
        call->semantic.type_name = "int";
        call->semantic.signature = it->second.signature;
        call->semantic.scope_depth = 0;
        call->semantic.declaration = it->second.decl;

        log("validated call " + call->callee + " with signature " + it->second.signature);
        for (auto* arg : call->args) {
            analyze_expr(arg);
        }
        return;
    }

    throw std::runtime_error("Unknown expression in semantic analysis");
}
