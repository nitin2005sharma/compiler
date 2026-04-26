#ifndef PARSER_HPP
#define PARSER_HPP

#include <vector>

#include "ast.hpp"
#include "diagnostics.hpp"
#include "lexer.hpp"
#include "trace.hpp"

class Parser {
    std::vector<Token> toks;
    size_t pos;
    TraceLogger* trace;

    Token peek() const {
        if (pos < toks.size()) {
            return toks[pos];
        }
        if (!toks.empty()) {
            return toks.back();
        }
        return Token{TokenType::END, "", 0, 1, 1};
    }

    Token consume() {
        if (pos < toks.size()) {
            return toks[pos++];
        }
        if (!toks.empty()) {
            pos = toks.size();
            return toks.back();
        }
        return Token{TokenType::END, "", 0, 1, 1};
    }

    bool accept(TokenType t) {
        if (peek().type == t) {
            consume();
            return true;
        }
        return false;
    }

    void expect(TokenType t, const std::string& msg);
    void log(const std::string& message) const;

public:
    Parser(const std::vector<Token>& tokens, TraceLogger* trace_logger = nullptr)
        : toks(tokens), pos(0), trace(trace_logger) {}

    ASTProgram* parse_program();

private:
    ASTNode* parse_decl();
    FuncDecl* parse_func();
    VarDecl* parse_vardecl();
    ASTStmt* parse_stmt();
    Block* parse_block();
    ASTStmt* parse_simple_stmt();
    ASTExpr* parse_expr();
    ASTExpr* parse_equality();
    ASTExpr* parse_relational();
    ASTExpr* parse_add();
    ASTExpr* parse_mul();
    ASTExpr* parse_unary();
    ASTExpr* parse_primary();
};

#endif
