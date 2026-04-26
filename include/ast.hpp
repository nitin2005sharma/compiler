#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <utility>
#include <vector>

#include "diagnostics.hpp"

struct ASTNode;

struct SemanticAnnotation {
    std::string type_name;
    std::string symbol_kind;
    std::string signature;
    int scope_depth = -1;
    const ASTNode* declaration = nullptr;
};

struct ASTNode {
    SemanticAnnotation semantic;
    SourceRange source_range;

    virtual ~ASTNode() {}
    virtual std::string to_dot_node(std::string& out, int& id) = 0;

    std::string to_dot() {
        std::string out = "digraph AST{node[shape=box];\n";
        int id = 0;
        to_dot_node(out, id);
        out += "}\n";
        return out;
    }

    std::string to_annotated_dot();
};

struct ASTExpr : ASTNode {};
struct ASTStmt : ASTNode {};

struct ASTProgram : ASTNode {
    std::vector<ASTNode*> decls;

    ~ASTProgram() override {
        for (auto* d : decls) {
            delete d;
        }
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct VarDecl : ASTStmt {
    std::string name;

    explicit VarDecl(std::string n)
        : name(std::move(n)) {}

    std::string to_dot_node(std::string& out, int& id) override;
};

struct ParamDecl : ASTNode {
    std::string name;

    explicit ParamDecl(std::string n)
        : name(std::move(n)) {}

    std::string to_dot_node(std::string& out, int& id) override;
};

struct FuncDecl : ASTNode {
    std::string name;
    std::vector<ParamDecl*> params;
    std::vector<ASTStmt*> body;

    explicit FuncDecl(std::string n)
        : name(std::move(n)) {}

    ~FuncDecl() override {
        for (auto* p : params) {
            delete p;
        }
        for (auto* s : body) {
            delete s;
        }
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct Block : ASTStmt {
    std::vector<ASTStmt*> stmts;

    ~Block() override {
        for (auto* s : stmts) {
            delete s;
        }
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct IfStmt : ASTStmt {
    ASTExpr* cond;
    ASTStmt* then_s;
    ASTStmt* else_s;

    IfStmt(ASTExpr* c, ASTStmt* t, ASTStmt* e)
        : cond(c), then_s(t), else_s(e) {}

    ~IfStmt() override {
        delete cond;
        delete then_s;
        delete else_s;
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct WhileStmt : ASTStmt {
    ASTExpr* cond;
    ASTStmt* body;

    WhileStmt(ASTExpr* c, ASTStmt* b)
        : cond(c), body(b) {}

    ~WhileStmt() override {
        delete cond;
        delete body;
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct ReturnStmt : ASTStmt {
    ASTExpr* expr;

    explicit ReturnStmt(ASTExpr* e)
        : expr(e) {}

    ~ReturnStmt() override {
        delete expr;
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct AssignStmt : ASTStmt {
    std::string name;
    ASTExpr* expr;

    AssignStmt(std::string n, ASTExpr* e)
        : name(std::move(n)), expr(e) {}

    ~AssignStmt() override {
        delete expr;
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct ExprStmt : ASTStmt {
    ASTExpr* expr;

    explicit ExprStmt(ASTExpr* e)
        : expr(e) {}

    ~ExprStmt() override {
        delete expr;
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct BinaryExpr : ASTExpr {
    std::string op;
    ASTExpr* lhs;
    ASTExpr* rhs;

    BinaryExpr(std::string o, ASTExpr* l, ASTExpr* r)
        : op(std::move(o)), lhs(l), rhs(r) {}

    ~BinaryExpr() override {
        delete lhs;
        delete rhs;
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

struct IntLiteral : ASTExpr {
    int value;

    explicit IntLiteral(int v)
        : value(v) {}

    std::string to_dot_node(std::string& out, int& id) override;
};

struct Ident : ASTExpr {
    std::string name;

    explicit Ident(std::string n)
        : name(std::move(n)) {}

    std::string to_dot_node(std::string& out, int& id) override;
};

struct CallExpr : ASTExpr {
    std::string callee;
    std::vector<ASTExpr*> args;

    explicit CallExpr(std::string name)
        : callee(std::move(name)) {}

    ~CallExpr() override {
        for (auto* arg : args) {
            delete arg;
        }
    }

    std::string to_dot_node(std::string& out, int& id) override;
};

#endif
