#include "lexer.hpp"

#include <sstream>

std::string token_type_name(TokenType type) {
    switch (type) {
        case TokenType::IDENT: return "IDENT";
        case TokenType::INT_LIT: return "INT_LIT";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::MUL: return "MUL";
        case TokenType::DIV: return "DIV";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::EQ: return "EQ";
        case TokenType::NEQ: return "NEQ";
        case TokenType::LT: return "LT";
        case TokenType::LE: return "LE";
        case TokenType::GT: return "GT";
        case TokenType::GE: return "GE";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::SEMI: return "SEMI";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::RETURN: return "RETURN";
        case TokenType::INT_KW: return "INT_KW";
        case TokenType::COMMA: return "COMMA";
        case TokenType::END: return "END";
    }
    return "UNKNOWN";
}

std::string token_to_string(const Token& token) {
    std::ostringstream os;
    os << token_type_name(token.type);

    if (!token.text.empty()) {
        os << "(\"" << token.text << "\")";
    }

    if (token.type == TokenType::INT_LIT) {
        os << "=" << token.int_value;
    }

    os << " @ line " << token.line;
    return os.str();
}

Token Lexer::next() {
    skip_ws();
    if (i >= s.size()) {
        return Token{TokenType::END, "", 0, line};
    }

    char c = s[i];
    unsigned char uc = static_cast<unsigned char>(c);

    if (isalpha(uc) || c == '_') {
        size_t j = i;
        while (j < s.size() && (isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_')) {
            ++j;
        }

        std::string w = s.substr(i, j - i);
        i = j;

        if (w == "if") return Token{TokenType::IF, w, 0, line};
        if (w == "else") return Token{TokenType::ELSE, w, 0, line};
        if (w == "while") return Token{TokenType::WHILE, w, 0, line};
        if (w == "return") return Token{TokenType::RETURN, w, 0, line};
        if (w == "int") return Token{TokenType::INT_KW, w, 0, line};
        return Token{TokenType::IDENT, w, 0, line};
    }

    if (isdigit(uc)) {
        size_t j = i;
        while (j < s.size() && isdigit(static_cast<unsigned char>(s[j]))) {
            ++j;
        }
        std::string num = s.substr(i, j - i);
        i = j;
        return Token{TokenType::INT_LIT, num, std::stoi(num), line};
    }

    if (starts("==")) {
        i += 2;
        return Token{TokenType::EQ, "==", 0, line};
    }
    if (starts("!=")) {
        i += 2;
        return Token{TokenType::NEQ, "!=", 0, line};
    }
    if (starts("<=")) {
        i += 2;
        return Token{TokenType::LE, "<=", 0, line};
    }
    if (starts(">=")) {
        i += 2;
        return Token{TokenType::GE, ">=", 0, line};
    }

    ++i;
    switch (c) {
        case '+': return Token{TokenType::PLUS, "+", 0, line};
        case '-': return Token{TokenType::MINUS, "-", 0, line};
        case '*': return Token{TokenType::MUL, "*", 0, line};
        case '/': return Token{TokenType::DIV, "/", 0, line};
        case '=': return Token{TokenType::ASSIGN, "=", 0, line};
        case '<': return Token{TokenType::LT, "<", 0, line};
        case '>': return Token{TokenType::GT, ">", 0, line};
        case '(': return Token{TokenType::LPAREN, "(", 0, line};
        case ')': return Token{TokenType::RPAREN, ")", 0, line};
        case '{': return Token{TokenType::LBRACE, "{", 0, line};
        case '}': return Token{TokenType::RBRACE, "}", 0, line};
        case ';': return Token{TokenType::SEMI, ";", 0, line};
        case ',': return Token{TokenType::COMMA, ",", 0, line};
        default: {
            std::ostringstream os;
            os << "Unexpected char '" << c << "' at line " << line;
            throw std::runtime_error(os.str());
        }
    }
}
