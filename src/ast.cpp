#include "ast.hpp"

#include <sstream>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::string escape_dot_label(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);

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

void emit_annotated_node(std::string& out, int id, const std::vector<std::string>& lines, const std::string& fill) {
    std::ostringstream label;
    for (size_t index = 0; index < lines.size(); ++index) {
        if (index != 0) {
            label << "\n";
        }
        label << lines[index];
    }

    out += "node" + std::to_string(id) +
           "[shape=box,style=\"rounded,filled\",fillcolor=\"" + fill +
           "\",label=\"" + escape_dot_label(label.str()) + "\"];\n";
}

void emit_annotated_edge(std::string& out,
                         int from,
                         int to,
                         const std::string& label = "",
                         const std::string& style = "") {
    out += "node" + std::to_string(from) + " -> node" + std::to_string(to);
    bool wrote_attr = false;
    if (!label.empty() || !style.empty()) {
        out += "[";
        if (!label.empty()) {
            out += "label=\"" + escape_dot_label(label) + "\"";
            wrote_attr = true;
        }
        if (!style.empty()) {
            if (wrote_attr) {
                out += ",";
            }
            out += "style=\"" + style + "\"";
        }
        out += "]";
    }
    out += ";\n";
}

void append_semantic_lines(const ASTNode* node, std::vector<std::string>& lines) {
    if (node->semantic.symbol_kind.empty() &&
        node->semantic.type_name.empty() &&
        node->semantic.signature.empty() &&
        node->semantic.scope_depth < 0) {
        return;
    }

    if (!node->semantic.symbol_kind.empty()) {
        lines.push_back("kind=" + node->semantic.symbol_kind);
    }
    if (!node->semantic.type_name.empty()) {
        lines.push_back("type=" + node->semantic.type_name);
    }
    if (!node->semantic.signature.empty()) {
        lines.push_back("signature=" + node->semantic.signature);
    }
    if (node->semantic.scope_depth >= 0) {
        lines.push_back("scope_depth=" + std::to_string(node->semantic.scope_depth));
    }
}

std::string semantic_link_label(const ASTNode* owner) {
    if (dynamic_cast<const CallExpr*>(owner) != nullptr) {
        return "calls";
    }
    if (dynamic_cast<const AssignStmt*>(owner) != nullptr) {
        return "writes";
    }
    if (dynamic_cast<const Ident*>(owner) != nullptr) {
        return "uses";
    }
    return "ref";
}

int annotated_node_for(const ASTNode* node,
                       std::string& out,
                       int& id,
                       std::unordered_map<const ASTNode*, int>& node_ids,
                       std::vector<std::tuple<int, const ASTNode*, const ASTNode*>>& semantic_refs) {
    const int my = id++;
    node_ids[node] = my;

    auto maybe_record_ref = [&](const ASTNode* owner) {
        if (owner->semantic.declaration != nullptr && owner->semantic.declaration != owner) {
            semantic_refs.emplace_back(my, owner, owner->semantic.declaration);
        }
    };

    if (auto* program = dynamic_cast<const ASTProgram*>(node)) {
        std::vector<std::string> lines = {
            "Program",
            "declarations=" + std::to_string(program->decls.size())
        };
        append_semantic_lines(program, lines);
        emit_annotated_node(out, my, lines, "#f9c74f");
        for (auto* decl : program->decls) {
            int child = annotated_node_for(decl, out, id, node_ids, semantic_refs);
            emit_annotated_edge(out, my, child, "decl");
        }
        return my;
    }

    if (auto* var = dynamic_cast<const VarDecl*>(node)) {
        std::vector<std::string> lines = {
            "VarDecl",
            "name=" + var->name
        };
        append_semantic_lines(var, lines);
        emit_annotated_node(out, my, lines, "#90be6d");
        return my;
    }

    if (auto* param = dynamic_cast<const ParamDecl*>(node)) {
        std::vector<std::string> lines = {
            "ParamDecl",
            "name=" + param->name
        };
        append_semantic_lines(param, lines);
        emit_annotated_node(out, my, lines, "#43aa8b");
        return my;
    }

    if (auto* func = dynamic_cast<const FuncDecl*>(node)) {
        std::vector<std::string> lines = {
            "Function",
            "name=" + func->name,
            "params=" + std::to_string(func->params.size()),
            "body_stmts=" + std::to_string(func->body.size())
        };
        append_semantic_lines(func, lines);
        emit_annotated_node(out, my, lines, "#577590");

        int params_node = id++;
        emit_annotated_node(
            out,
            params_node,
            std::vector<std::string>{
                "Parameters",
                "count=" + std::to_string(func->params.size())
            },
            "#8ecae6"
        );
        emit_annotated_edge(out, my, params_node, "params");

        for (auto* param : func->params) {
            int param_node = annotated_node_for(param, out, id, node_ids, semantic_refs);
            emit_annotated_edge(out, params_node, param_node);
        }

        int body_node = id++;
        emit_annotated_node(
            out,
            body_node,
            std::vector<std::string>{
                "Body",
                "statements=" + std::to_string(func->body.size())
            },
            "#8ecae6"
        );
        emit_annotated_edge(out, my, body_node, "body");

        for (auto* stmt : func->body) {
            int child = annotated_node_for(stmt, out, id, node_ids, semantic_refs);
            emit_annotated_edge(out, body_node, child, "stmt");
        }

        return my;
    }

    if (auto* block = dynamic_cast<const Block*>(node)) {
        std::vector<std::string> lines = {
            "Block",
            "statements=" + std::to_string(block->stmts.size())
        };
        append_semantic_lines(block, lines);
        emit_annotated_node(out, my, lines, "#d9ed92");
        for (auto* stmt : block->stmts) {
            int child = annotated_node_for(stmt, out, id, node_ids, semantic_refs);
            emit_annotated_edge(out, my, child, "stmt");
        }
        return my;
    }

    if (auto* if_stmt = dynamic_cast<const IfStmt*>(node)) {
        std::vector<std::string> lines = {
            "IfStmt",
            std::string("has_else=") + (if_stmt->else_s ? "true" : "false")
        };
        append_semantic_lines(if_stmt, lines);
        emit_annotated_node(out, my, lines, "#f4a261");
        int cond_node = annotated_node_for(if_stmt->cond, out, id, node_ids, semantic_refs);
        emit_annotated_edge(out, my, cond_node, "cond");
        int then_node = annotated_node_for(if_stmt->then_s, out, id, node_ids, semantic_refs);
        emit_annotated_edge(out, my, then_node, "then");
        if (if_stmt->else_s) {
            int else_node = annotated_node_for(if_stmt->else_s, out, id, node_ids, semantic_refs);
            emit_annotated_edge(out, my, else_node, "else");
        }
        return my;
    }

    if (auto* while_stmt = dynamic_cast<const WhileStmt*>(node)) {
        std::vector<std::string> lines = {
            "WhileStmt",
            "kind=loop"
        };
        append_semantic_lines(while_stmt, lines);
        emit_annotated_node(out, my, lines, "#f4a261");
        int cond_node = annotated_node_for(while_stmt->cond, out, id, node_ids, semantic_refs);
        emit_annotated_edge(out, my, cond_node, "cond");
        int body_node = annotated_node_for(while_stmt->body, out, id, node_ids, semantic_refs);
        emit_annotated_edge(out, my, body_node, "body");
        return my;
    }

    if (auto* ret = dynamic_cast<const ReturnStmt*>(node)) {
        std::vector<std::string> lines = {
            "ReturnStmt",
            std::string("has_value=") + (ret->expr ? "true" : "false")
        };
        append_semantic_lines(ret, lines);
        emit_annotated_node(out, my, lines, "#e76f51");
        if (ret->expr) {
            int expr_node = annotated_node_for(ret->expr, out, id, node_ids, semantic_refs);
            emit_annotated_edge(out, my, expr_node, "value");
        }
        return my;
    }

    if (auto* assign = dynamic_cast<const AssignStmt*>(node)) {
        std::vector<std::string> lines = {
            "AssignStmt",
            "target=" + assign->name
        };
        append_semantic_lines(assign, lines);
        emit_annotated_node(out, my, lines, "#e9c46a");
        maybe_record_ref(assign);
        int expr_node = annotated_node_for(assign->expr, out, id, node_ids, semantic_refs);
        emit_annotated_edge(out, my, expr_node, "value");
        return my;
    }

    if (auto* expr_stmt = dynamic_cast<const ExprStmt*>(node)) {
        std::vector<std::string> lines = {
            "ExprStmt"
        };
        append_semantic_lines(expr_stmt, lines);
        emit_annotated_node(out, my, lines, "#e9c46a");
        int expr_node = annotated_node_for(expr_stmt->expr, out, id, node_ids, semantic_refs);
        emit_annotated_edge(out, my, expr_node, "expr");
        return my;
    }

    if (auto* binary = dynamic_cast<const BinaryExpr*>(node)) {
        std::vector<std::string> lines = {
            "BinaryExpr",
            "op=" + binary->op
        };
        append_semantic_lines(binary, lines);
        emit_annotated_node(out, my, lines, "#adb5bd");
        int left_node = annotated_node_for(binary->lhs, out, id, node_ids, semantic_refs);
        emit_annotated_edge(out, my, left_node, "lhs");
        int right_node = annotated_node_for(binary->rhs, out, id, node_ids, semantic_refs);
        emit_annotated_edge(out, my, right_node, "rhs");
        return my;
    }

    if (auto* literal = dynamic_cast<const IntLiteral*>(node)) {
        std::vector<std::string> lines = {
            "IntLiteral",
            "value=" + std::to_string(literal->value)
        };
        append_semantic_lines(literal, lines);
        emit_annotated_node(out, my, lines, "#ced4da");
        return my;
    }

    if (auto* ident = dynamic_cast<const Ident*>(node)) {
        std::vector<std::string> lines = {
            "Identifier",
            "name=" + ident->name
        };
        append_semantic_lines(ident, lines);
        emit_annotated_node(out, my, lines, "#ced4da");
        maybe_record_ref(ident);
        return my;
    }

    if (auto* call = dynamic_cast<const CallExpr*>(node)) {
        std::vector<std::string> lines = {
            "CallExpr",
            "callee=" + call->callee,
            "args=" + std::to_string(call->args.size())
        };
        append_semantic_lines(call, lines);
        emit_annotated_node(out, my, lines, "#bde0fe");
        maybe_record_ref(call);
        for (size_t index = 0; index < call->args.size(); ++index) {
            int arg_node = annotated_node_for(call->args[index], out, id, node_ids, semantic_refs);
            emit_annotated_edge(out, my, arg_node, "arg" + std::to_string(index));
        }
        return my;
    }

    emit_annotated_node(out, my, std::vector<std::string>{"UnknownNode"}, "#ffcad4");
    return my;
}

}  // namespace

std::string ASTNode::to_annotated_dot() {
    std::string out = "digraph AnnotatedAST{graph[pad=0.3,nodesep=0.4,ranksep=0.55];\n";
    std::unordered_map<const ASTNode*, int> node_ids;
    std::vector<std::tuple<int, const ASTNode*, const ASTNode*>> semantic_refs;
    int id = 0;
    annotated_node_for(this, out, id, node_ids, semantic_refs);

    for (const auto& ref : semantic_refs) {
        auto it = node_ids.find(std::get<2>(ref));
        if (it == node_ids.end()) {
            continue;
        }
        emit_annotated_edge(out, std::get<0>(ref), it->second, semantic_link_label(std::get<1>(ref)), "dashed");
    }

    out += "}\n";
    return out;
}

std::string ASTProgram::to_dot_node(std::string& out, int& id) {
    int my = id++;
    std::ostringstream os;
    os << "node" << my << "[label=\"Program\"];\n";
    out += os.str();

    for (auto* d : decls) {
        int child = id;
        d->to_dot_node(out, id);
        std::ostringstream edge;
        edge << "node" << my << " -> node" << child << ";\n";
        out += edge.str();
    }

    return "node" + std::to_string(my);
}

std::string VarDecl::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"VarDecl: " + name + "\"];\n";
    return "node" + std::to_string(my);
}

std::string ParamDecl::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Param: " + name + "\"];\n";
    return "node" + std::to_string(my);
}

std::string FuncDecl::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Func: " + name + "\"];\n";

    int params_node = id++;
    out += "node" + std::to_string(params_node) + "[label=\"Params\"];\n";
    out += "node" + std::to_string(my) + " -> node" + std::to_string(params_node) + ";\n";

    for (auto* p : params) {
        int pn = id;
        p->to_dot_node(out, id);
        out += "node" + std::to_string(params_node) + " -> node" + std::to_string(pn) + ";\n";
    }

    int body_node = id++;
    out += "node" + std::to_string(body_node) + "[label=\"Body\"];\n";
    out += "node" + std::to_string(my) + " -> node" + std::to_string(body_node) + ";\n";

    for (auto* s : body) {
        int child = id;
        s->to_dot_node(out, id);
        out += "node" + std::to_string(body_node) + " -> node" + std::to_string(child) + ";\n";
    }

    return "node" + std::to_string(my);
}

std::string Block::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Block\"];\n";

    for (auto* s : stmts) {
        int child = id;
        s->to_dot_node(out, id);
        out += "node" + std::to_string(my) + " -> node" + std::to_string(child) + ";\n";
    }

    return "node" + std::to_string(my);
}

std::string IfStmt::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"If\"];\n";

    int cond_node = id;
    cond->to_dot_node(out, id);
    out += "node" + std::to_string(my) + " -> node" + std::to_string(cond_node) + "[label=\"cond\"];\n";

    int then_node = id;
    then_s->to_dot_node(out, id);
    out += "node" + std::to_string(my) + " -> node" + std::to_string(then_node) + "[label=\"then\"];\n";

    if (else_s) {
        int else_node = id;
        else_s->to_dot_node(out, id);
        out += "node" + std::to_string(my) + " -> node" + std::to_string(else_node) + "[label=\"else\"];\n";
    }

    return "node" + std::to_string(my);
}

std::string WhileStmt::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"While\"];\n";

    int cond_node = id;
    cond->to_dot_node(out, id);
    out += "node" + std::to_string(my) + " -> node" + std::to_string(cond_node) + "[label=\"cond\"];\n";

    int body_node = id;
    body->to_dot_node(out, id);
    out += "node" + std::to_string(my) + " -> node" + std::to_string(body_node) + "[label=\"body\"];\n";

    return "node" + std::to_string(my);
}

std::string ReturnStmt::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Return\"];\n";

    if (expr) {
        int expr_node = id;
        expr->to_dot_node(out, id);
        out += "node" + std::to_string(my) + " -> node" + std::to_string(expr_node) + ";\n";
    }

    return "node" + std::to_string(my);
}

std::string AssignStmt::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Assign: " + name + "\"];\n";

    int expr_node = id;
    expr->to_dot_node(out, id);
    out += "node" + std::to_string(my) + " -> node" + std::to_string(expr_node) + ";\n";

    return "node" + std::to_string(my);
}

std::string ExprStmt::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"ExprStmt\"];\n";

    int expr_node = id;
    expr->to_dot_node(out, id);
    out += "node" + std::to_string(my) + " -> node" + std::to_string(expr_node) + ";\n";

    return "node" + std::to_string(my);
}

std::string BinaryExpr::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Op: " + op + "\"];\n";

    int left_node = id;
    lhs->to_dot_node(out, id);
    out += "node" + std::to_string(my) + " -> node" + std::to_string(left_node) + "[label=\"lhs\"];\n";

    int right_node = id;
    rhs->to_dot_node(out, id);
    out += "node" + std::to_string(my) + " -> node" + std::to_string(right_node) + "[label=\"rhs\"];\n";

    return "node" + std::to_string(my);
}

std::string IntLiteral::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Int: " + std::to_string(value) + "\"];\n";
    return "node" + std::to_string(my);
}

std::string Ident::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Ident: " + name + "\"];\n";
    return "node" + std::to_string(my);
}

std::string CallExpr::to_dot_node(std::string& out, int& id) {
    int my = id++;
    out += "node" + std::to_string(my) + "[label=\"Call: " + callee + "\"];\n";

    for (auto* arg : args) {
        int arg_node = id;
        arg->to_dot_node(out, id);
        out += "node" + std::to_string(my) + " -> node" + std::to_string(arg_node) + ";\n";
    }

    return "node" + std::to_string(my);
}
