#include "parser.hpp"

#include <sstream>
#include <stdexcept>

namespace {

SourceRange token_range(const Token& token) {
    return SourceRange{
        token.line,
        token.column,
        static_cast<int>(token.text.empty() ? 1 : token.text.size())
    };
}

std::string describe_token(const Token& token) {
    std::ostringstream os;
    os << token_type_name(token.type);
    if (!token.text.empty()) {
        os << " '" << token.text << "'";
    }
    return os.str();
}

template <typename T>
T* mark_node(T* node, const Token& token) {
    node->source_range = token_range(token);
    return node;
}

template <typename T>
T* mark_node(T* node, const SourceRange& range) {
    node->source_range = range;
    return node;
}

}  // namespace

void Parser::log(const std::string& message) const {
    if (trace != nullptr) {
        trace->log("PARSER", message);
    }
}

void Parser::expect(TokenType t, const std::string& msg) {
    if (!accept(t)) {
        Token actual = peek();
        std::ostringstream os;
        os << msg << " (expected " << token_type_name(t)
           << ", found " << describe_token(actual) << ")";
        throw CompileError("Parse", os.str(), token_range(actual));
    }
}

ASTProgram* Parser::parse_program() {
    log("begin parse_program");
    ASTProgram* p = new ASTProgram();

    while (peek().type != TokenType::END) {
        p->decls.push_back(parse_decl());
    }

    log("end parse_program with " + std::to_string(p->decls.size()) + " declarations");
    return p;
}

ASTNode* Parser::parse_decl() {
    log("parse_decl starting at " + token_to_string(peek()));
    if (peek().type == TokenType::INT_KW) {
        size_t save = pos;
        consume();
        if (peek().type != TokenType::IDENT) {
            throw CompileError("Parse", "expected identifier after int", token_range(peek()));
        }
        std::string name = consume().text;
        if (peek().type == TokenType::LPAREN) {
            pos = save;
            log("dispatching declaration as function: " + name);
            return parse_func();
        }

        pos = save;
        log("dispatching declaration as variable: " + name);
        return parse_vardecl();
    }

    throw CompileError("Parse", "expected declaration", token_range(peek()));
}

VarDecl* Parser::parse_vardecl() {
    Token intToken = peek();
    expect(TokenType::INT_KW, "expected int");
    if (peek().type != TokenType::IDENT) {
        throw CompileError("Parse", "expected identifier", token_range(peek()));
    }

    Token nameToken = consume();
    std::string name = nameToken.text;
    expect(TokenType::SEMI, "expected ;");
    log("created VarDecl for " + name);
    return mark_node(new VarDecl(name), SourceRange{intToken.line, intToken.column, static_cast<int>(name.size())});
}

FuncDecl* Parser::parse_func() {
    Token intToken = peek();
    expect(TokenType::INT_KW, "expected int");
    if (peek().type != TokenType::IDENT) {
        throw CompileError("Parse", "expected function name", token_range(peek()));
    }

    Token nameToken = consume();
    std::string name = nameToken.text;
    log("parse_func " + name);
    expect(TokenType::LPAREN, "expected (");

    FuncDecl* f = mark_node(new FuncDecl(name), SourceRange{intToken.line, intToken.column, static_cast<int>(name.size())});
    if (peek().type != TokenType::RPAREN) {
        while (true) {
            Token paramTypeToken = peek();
            expect(TokenType::INT_KW, "expected parameter type");
            if (peek().type != TokenType::IDENT) {
                throw CompileError("Parse", "expected parameter name", token_range(peek()));
            }
            Token paramToken = consume();
            std::string param = paramToken.text;
            f->params.push_back(mark_node(new ParamDecl(param), SourceRange{paramTypeToken.line, paramTypeToken.column, static_cast<int>(param.size())}));
            log("added parameter " + param + " to " + name);
            if (!accept(TokenType::COMMA)) {
                break;
            }
        }
    }

    expect(TokenType::RPAREN, "expected )");
    Block* b = parse_block();
    for (auto* s : b->stmts) {
        f->body.push_back(s);
    }
    b->stmts.clear();
    delete b;

    log("completed function " + name + " with " + std::to_string(f->body.size()) + " statements");
    return f;
}

Block* Parser::parse_block() {
    Token blockToken = peek();
    expect(TokenType::LBRACE, "expected {");
    log("enter block");

    Block* b = mark_node(new Block(), blockToken);
    while (peek().type != TokenType::RBRACE && peek().type != TokenType::END) {
        b->stmts.push_back(parse_stmt());
    }

    expect(TokenType::RBRACE, "expected }");
    log("leave block with " + std::to_string(b->stmts.size()) + " statements");
    return b;
}

ASTStmt* Parser::parse_stmt() {
    if (peek().type == TokenType::INT_KW) {
        return parse_vardecl();
    }
    if (peek().type == TokenType::LBRACE) {
        return parse_block();
    }
    if (peek().type == TokenType::IF) {
        Token ifToken = consume();
        log("parse if statement");
        expect(TokenType::LPAREN, "expected (");
        ASTExpr* c = parse_expr();
        expect(TokenType::RPAREN, "expected )");
        ASTStmt* th = parse_stmt();
        ASTStmt* el = nullptr;
        if (accept(TokenType::ELSE)) {
            log("if statement has else branch");
            el = parse_stmt();
        }
        return mark_node(new IfStmt(c, th, el), ifToken);
    }
    if (peek().type == TokenType::WHILE) {
        Token whileToken = consume();
        log("parse while statement");
        expect(TokenType::LPAREN, "expected (");
        ASTExpr* c = parse_expr();
        expect(TokenType::RPAREN, "expected )");
        ASTStmt* bd = parse_stmt();
        return mark_node(new WhileStmt(c, bd), whileToken);
    }
    if (peek().type == TokenType::RETURN) {
        Token returnToken = consume();
        log("parse return statement");
        ASTExpr* e = nullptr;
        if (peek().type != TokenType::SEMI) {
            e = parse_expr();
        }
        expect(TokenType::SEMI, "expected ;");
        return mark_node(new ReturnStmt(e), returnToken);
    }
    return parse_simple_stmt();
}

ASTStmt* Parser::parse_simple_stmt() {
    ASTExpr* e = parse_expr();
    if (peek().type == TokenType::ASSIGN) {
        Ident* id = dynamic_cast<Ident*>(e);
        if (!id) {
            SourceRange badRange = e->source_range;
            delete e;
            throw CompileError("Parse", "left side of assignment must be an identifier", badRange);
        }

        std::string name = id->name;
        SourceRange assignRange = id->source_range;
        delete e;
        consume();
        ASTExpr* rhs = parse_expr();
        expect(TokenType::SEMI, "expected ;");
        log("created assignment to " + name);
        return mark_node(new AssignStmt(name, rhs), assignRange);
    }

    expect(TokenType::SEMI, "expected ;");
    log("created expression statement");
    return mark_node(new ExprStmt(e), e->source_range);
}

ASTExpr* Parser::parse_expr() {
    return parse_equality();
}

ASTExpr* Parser::parse_equality() {
    ASTExpr* left = parse_relational();
    while (peek().type == TokenType::EQ || peek().type == TokenType::NEQ) {
        Token opToken = consume();
        std::string op = opToken.text;
        ASTExpr* right = parse_relational();
        log("created equality op " + op);
        left = mark_node(new BinaryExpr(op, left, right), opToken);
    }
    return left;
}

ASTExpr* Parser::parse_relational() {
    ASTExpr* left = parse_add();
    while (peek().type == TokenType::LT || peek().type == TokenType::LE ||
           peek().type == TokenType::GT || peek().type == TokenType::GE) {
        Token opToken = consume();
        std::string op = opToken.text;
        ASTExpr* right = parse_add();
        log("created relational op " + op);
        left = mark_node(new BinaryExpr(op, left, right), opToken);
    }
    return left;
}

ASTExpr* Parser::parse_add() {
    ASTExpr* left = parse_mul();
    while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
        Token opToken = consume();
        std::string op = opToken.text;
        ASTExpr* right = parse_mul();
        log("created additive op " + op);
        left = mark_node(new BinaryExpr(op, left, right), opToken);
    }
    return left;
}

ASTExpr* Parser::parse_mul() {
    ASTExpr* left = parse_unary();
    while (peek().type == TokenType::MUL || peek().type == TokenType::DIV) {
        Token opToken = consume();
        std::string op = opToken.text;
        ASTExpr* right = parse_unary();
        log("created multiplicative op " + op);
        left = mark_node(new BinaryExpr(op, left, right), opToken);
    }
    return left;
}

ASTExpr* Parser::parse_unary() {
    if (peek().type == TokenType::PLUS) {
        consume();
        log("parse unary plus");
        return parse_unary();
    }
    if (peek().type == TokenType::MINUS) {
        Token minusToken = consume();
        log("parse unary minus");
        ASTExpr* r = parse_unary();
        return mark_node(new BinaryExpr("-", mark_node(new IntLiteral(0), minusToken), r), minusToken);
    }
    return parse_primary();
}

ASTExpr* Parser::parse_primary() {
    if (peek().type == TokenType::INT_LIT) {
        Token literalToken = consume();
        int v = literalToken.int_value;
        log("created int literal " + std::to_string(v));
        return mark_node(new IntLiteral(v), literalToken);
    }

    if (peek().type == TokenType::IDENT) {
        Token identToken = consume();
        std::string n = identToken.text;
        if (accept(TokenType::LPAREN)) {
            CallExpr* call = mark_node(new CallExpr(n), identToken);
            if (peek().type != TokenType::RPAREN) {
                while (true) {
                    call->args.push_back(parse_expr());
                    if (!accept(TokenType::COMMA)) {
                        break;
                    }
                }
            }
            expect(TokenType::RPAREN, "expected )");
            log("created call expr " + n + " with " + std::to_string(call->args.size()) + " args");
            return call;
        }
        log("created identifier " + n);
        return mark_node(new Ident(n), identToken);
    }

    if (peek().type == TokenType::LPAREN) {
        consume();
        ASTExpr* e = parse_expr();
        expect(TokenType::RPAREN, "expected )");
        log("parsed parenthesized expression");
        return e;
    }

    throw CompileError("Parse", "unexpected token in expression", token_range(peek()));
}
