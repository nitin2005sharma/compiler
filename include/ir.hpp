#ifndef IR_HPP
#define IR_HPP

#include <string>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "trace.hpp"

struct Quad {
    std::string op;
    std::string a;
    std::string b;
    std::string res;

    Quad(std::string o, std::string aa = "", std::string bb = "", std::string r = "")
        : op(std::move(o)), a(std::move(aa)), b(std::move(bb)), res(std::move(r)) {}

    std::string to_string() const;
};

struct IRProgram {
    std::vector<Quad> quads;
};

class IRGenerator {
    int tmpCnt = 0;
    int lblCnt = 0;
    TraceLogger* trace;

    std::string new_tmp() { return "t" + std::to_string(tmpCnt++); }
    std::string new_lbl() { return "L" + std::to_string(lblCnt++); }
    void emit(IRProgram& ir, const Quad& q) const;
    bool definitely_returns(ASTStmt* s) const;

public:
    explicit IRGenerator(TraceLogger* trace_logger = nullptr)
        : trace(trace_logger) {}

    IRProgram generate(ASTProgram* prog);

private:
    void gen_node(ASTNode* n, IRProgram& ir);
    void gen_func(FuncDecl* f, IRProgram& ir);
    std::string gen_stmt(ASTStmt* s, IRProgram& ir);
    std::string gen_expr(ASTExpr* e, IRProgram& ir);
};

#endif
