#ifndef LEXER_HPP
#define LEXER_HPP

#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

#include "diagnostics.hpp"

enum class TokenType {
    IDENT,
    INT_LIT,
    PLUS,
    MINUS,
    MUL,
    DIV,
    ASSIGN,
    EQ,
    NEQ,
    LT,
    LE,
    GT,
    GE,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    SEMI,
    IF,
    ELSE,
    WHILE,
    RETURN,
    INT_KW,
    COMMA,
    END
};

struct Token {
    TokenType type;
    std::string text;
    int int_value;
    int line;
    int column;
};

class Lexer {
    std::string s;
    size_t i;
    int line;
    int column;

    void skip_ws();
    bool starts(const std::string& p) const;
    void advance();

public:
    explicit Lexer(const std::string& src)
        : s(src), i(0), line(1), column(1) {}

    Token next();
};

std::string token_type_name(TokenType type);
std::string token_to_string(const Token& token);

#endif
