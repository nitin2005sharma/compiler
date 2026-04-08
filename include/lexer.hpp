#ifndef LEXER_HPP
#define LEXER_HPP

#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

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
};

class Lexer {
    std::string s;
    size_t i;
    int line;

    void skip_ws() {
        while (i < s.size()) {
            if (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n') {
                if (s[i] == '\n') {
                    ++line;
                }
                ++i;
                continue;
            }

            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
                i += 2;
                while (i < s.size() && s[i] != '\n') {
                    ++i;
                }
                continue;
            }

            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
                i += 2;
                bool closed = false;
                while (i < s.size()) {
                    if (s[i] == '\n') {
                        ++line;
                    }
                    if (i + 1 < s.size() && s[i] == '*' && s[i + 1] == '/') {
                        i += 2;
                        closed = true;
                        break;
                    }
                    ++i;
                }
                if (!closed) {
                    throw std::runtime_error("Unterminated block comment at line " + std::to_string(line));
                }
                continue;
            }

            break;
        }
    }

    bool starts(const std::string& p) const {
        return s.substr(i, p.size()) == p;
    }

public:
    explicit Lexer(const std::string& src)
        : s(src), i(0), line(1) {}

    Token next();
};

std::string token_type_name(TokenType type);
std::string token_to_string(const Token& token);

#endif
