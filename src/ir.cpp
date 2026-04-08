#include "ir.hpp"

#include <sstream>
#include <stdexcept>

std::string Quad::to_string() const {
    std::ostringstream os;
    if (op == "label") {
        os << res << ":";
    } else if (op == "goto") {
        os << "goto " << res;
    } else if (op == "ifFalse") {
        os << "ifFalse " << a << " goto " << res;
    } else if (op == "ret") {
        os << "ret " << a;
    } else if (op == "mov") {
        os << res << " = " << a;
    } else if (op == "param") {
        os << "param " << a;
    } else if (op == "arg") {
        os << res << " = arg " << a;
    } else if (op == "call") {
        os << res << " = call " << a << ", " << b;
    } else {
        os << res << " = " << a << " " << op << " " << b;
    }
    return os.str();
}

void IRGenerator::emit(IRProgram& ir, const Quad& q) const {
    ir.quads.push_back(q);
    if (trace != nullptr) {
        trace->log("IR", q.to_string());
    }
}

bool IRGenerator::definitely_returns(ASTStmt* s) const {
    if (dynamic_cast<ReturnStmt*>(s) != nullptr) {
        return true;
    }

    if (auto* block = dynamic_cast<Block*>(s)) {
        return !block->stmts.empty() && definitely_returns(block->stmts.back());
    }

    if (auto* if_stmt = dynamic_cast<IfStmt*>(s)) {
        return if_stmt->else_s != nullptr &&
               definitely_returns(if_stmt->then_s) &&
               definitely_returns(if_stmt->else_s);
    }

    return false;
}

IRProgram IRGenerator::generate(ASTProgram* prog) {
    IRProgram ir;
    tmpCnt = 0;
    lblCnt = 0;
    for (auto* d : prog->decls) {
        gen_node(d, ir);
    }
    return ir;
}

void IRGenerator::gen_node(ASTNode* n, IRProgram& ir) {
    if (auto* f = dynamic_cast<FuncDecl*>(n)) {
        gen_func(f, ir);
    }
}

void IRGenerator::gen_func(FuncDecl* f, IRProgram& ir) {
    emit(ir, Quad("label", "", "", f->name));

    for (size_t index = 0; index < f->params.size(); ++index) {
        emit(ir, Quad("arg", std::to_string(index), "", f->params[index]->name));
    }

    for (auto* s : f->body) {
        gen_stmt(s, ir);
    }

    if (f->body.empty() || !definitely_returns(f->body.back())) {
        emit(ir, Quad("ret", "0", "", ""));
    }
}

std::string IRGenerator::gen_stmt(ASTStmt* s, IRProgram& ir) {
    if (dynamic_cast<VarDecl*>(s) != nullptr) {
        return "";
    }

    if (auto* b = dynamic_cast<Block*>(s)) {
        for (auto* ss : b->stmts) {
            gen_stmt(ss, ir);
        }
        return "";
    }

    if (auto* is = dynamic_cast<IfStmt*>(s)) {
        std::string cond = gen_expr(is->cond, ir);
        std::string else_label = new_lbl();
        std::string end_label = new_lbl();
        emit(ir, Quad("ifFalse", cond, "", else_label));
        gen_stmt(is->then_s, ir);
        emit(ir, Quad("goto", "", "", end_label));
        emit(ir, Quad("label", "", "", else_label));
        if (is->else_s) {
            gen_stmt(is->else_s, ir);
        }
        emit(ir, Quad("label", "", "", end_label));
        return "";
    }

    if (auto* ws = dynamic_cast<WhileStmt*>(s)) {
        std::string start_label = new_lbl();
        std::string end_label = new_lbl();
        emit(ir, Quad("label", "", "", start_label));
        std::string cond = gen_expr(ws->cond, ir);
        emit(ir, Quad("ifFalse", cond, "", end_label));
        gen_stmt(ws->body, ir);
        emit(ir, Quad("goto", "", "", start_label));
        emit(ir, Quad("label", "", "", end_label));
        return "";
    }

    if (auto* rs = dynamic_cast<ReturnStmt*>(s)) {
        std::string val = rs->expr ? gen_expr(rs->expr, ir) : std::string("0");
        emit(ir, Quad("ret", val, "", ""));
        return "";
    }

    if (auto* as = dynamic_cast<AssignStmt*>(s)) {
        std::string rhs = gen_expr(as->expr, ir);
        emit(ir, Quad("mov", rhs, "", as->name));
        return "";
    }

    if (auto* es = dynamic_cast<ExprStmt*>(s)) {
        gen_expr(es->expr, ir);
        return "";
    }

    throw std::runtime_error("Unknown stmt in IR gen");
}

std::string IRGenerator::gen_expr(ASTExpr* e, IRProgram& ir) {
    if (auto* il = dynamic_cast<IntLiteral*>(e)) {
        return std::to_string(il->value);
    }

    if (auto* id = dynamic_cast<Ident*>(e)) {
        return id->name;
    }

    if (auto* be = dynamic_cast<BinaryExpr*>(e)) {
        std::string l = gen_expr(be->lhs, ir);
        std::string r = gen_expr(be->rhs, ir);
        std::string t = new_tmp();
        emit(ir, Quad(be->op, l, r, t));
        return t;
    }

    if (auto* call = dynamic_cast<CallExpr*>(e)) {
        for (auto* arg : call->args) {
            emit(ir, Quad("param", gen_expr(arg, ir), "", ""));
        }
        std::string t = new_tmp();
        emit(ir, Quad("call", call->callee, std::to_string(call->args.size()), t));
        return t;
    }

    throw std::runtime_error("Unknown expr in IR gen");
}
