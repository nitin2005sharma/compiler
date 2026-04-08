#include "parser.hpp"

#include <sstream>
#include <stdexcept>

void Parser::log(const std::string& message) const {
    if (trace != nullptr) {
        trace->log("PARSER", message);
    }
}

void Parser::expect(TokenType t, const std::string& msg) {
    if (!accept(t)) {
        std::ostringstream os;
        os << "Parse error at line " << peek().line << ": " << msg
           << " (found " << token_type_name(peek().type) << ")";
        throw std::runtime_error(os.str());
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
            throw std::runtime_error("Expected identifier after int");
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

    throw std::runtime_error("Expected declaration");
}

VarDecl* Parser::parse_vardecl() {
    expect(TokenType::INT_KW, "expected int");
    if (peek().type != TokenType::IDENT) {
        throw std::runtime_error("Expected identifier");
    }

    std::string name = consume().text;
    expect(TokenType::SEMI, "expected ;");
    log("created VarDecl for " + name);
    return new VarDecl(name);
}

FuncDecl* Parser::parse_func() {
    expect(TokenType::INT_KW, "expected int");
    if (peek().type != TokenType::IDENT) {
        throw std::runtime_error("Expected function name");
    }

    std::string name = consume().text;
    log("parse_func " + name);
    expect(TokenType::LPAREN, "expected (");

    FuncDecl* f = new FuncDecl(name);
    if (peek().type != TokenType::RPAREN) {
        while (true) {
            expect(TokenType::INT_KW, "expected parameter type");
            if (peek().type != TokenType::IDENT) {
                throw std::runtime_error("expected param name");
            }
            std::string param = consume().text;
            f->params.push_back(new ParamDecl(param));
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
    expect(TokenType::LBRACE, "expected {");
    log("enter block");

    Block* b = new Block();
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
        consume();
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
        return new IfStmt(c, th, el);
    }
    if (peek().type == TokenType::WHILE) {
        consume();
        log("parse while statement");
        expect(TokenType::LPAREN, "expected (");
        ASTExpr* c = parse_expr();
        expect(TokenType::RPAREN, "expected )");
        ASTStmt* bd = parse_stmt();
        return new WhileStmt(c, bd);
    }
    if (peek().type == TokenType::RETURN) {
        consume();
        log("parse return statement");
        ASTExpr* e = nullptr;
        if (peek().type != TokenType::SEMI) {
            e = parse_expr();
        }
        expect(TokenType::SEMI, "expected ;");
        return new ReturnStmt(e);
    }
    return parse_simple_stmt();
}

ASTStmt* Parser::parse_simple_stmt() {
    ASTExpr* e = parse_expr();
    if (peek().type == TokenType::ASSIGN) {
        Ident* id = dynamic_cast<Ident*>(e);
        if (!id) {
            delete e;
            throw std::runtime_error("Left side of assignment must be identifier");
        }

        std::string name = id->name;
        delete e;
        consume();
        ASTExpr* rhs = parse_expr();
        expect(TokenType::SEMI, "expected ;");
        log("created assignment to " + name);
        return new AssignStmt(name, rhs);
    }

    expect(TokenType::SEMI, "expected ;");
    log("created expression statement");
    return new ExprStmt(e);
}

ASTExpr* Parser::parse_expr() {
    return parse_equality();
}

ASTExpr* Parser::parse_equality() {
    ASTExpr* left = parse_relational();
    while (peek().type == TokenType::EQ || peek().type == TokenType::NEQ) {
        std::string op = consume().text;
        ASTExpr* right = parse_relational();
        log("created equality op " + op);
        left = new BinaryExpr(op, left, right);
    }
    return left;
}

ASTExpr* Parser::parse_relational() {
    ASTExpr* left = parse_add();
    while (peek().type == TokenType::LT || peek().type == TokenType::LE ||
           peek().type == TokenType::GT || peek().type == TokenType::GE) {
        std::string op = consume().text;
        ASTExpr* right = parse_add();
        log("created relational op " + op);
        left = new BinaryExpr(op, left, right);
    }
    return left;
}

ASTExpr* Parser::parse_add() {
    ASTExpr* left = parse_mul();
    while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
        std::string op = consume().text;
        ASTExpr* right = parse_mul();
        log("created additive op " + op);
        left = new BinaryExpr(op, left, right);
    }
    return left;
}

ASTExpr* Parser::parse_mul() {
    ASTExpr* left = parse_unary();
    while (peek().type == TokenType::MUL || peek().type == TokenType::DIV) {
        std::string op = consume().text;
        ASTExpr* right = parse_unary();
        log("created multiplicative op " + op);
        left = new BinaryExpr(op, left, right);
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
        consume();
        log("parse unary minus");
        ASTExpr* r = parse_unary();
        return new BinaryExpr("-", new IntLiteral(0), r);
    }
    return parse_primary();
}

ASTExpr* Parser::parse_primary() {
    if (peek().type == TokenType::INT_LIT) {
        int v = consume().int_value;
        log("created int literal " + std::to_string(v));
        return new IntLiteral(v);
    }

    if (peek().type == TokenType::IDENT) {
        std::string n = consume().text;
        if (accept(TokenType::LPAREN)) {
            CallExpr* call = new CallExpr(n);
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
        return new Ident(n);
    }

    if (peek().type == TokenType::LPAREN) {
        consume();
        ASTExpr* e = parse_expr();
        expect(TokenType::RPAREN, "expected )");
        log("parsed parenthesized expression");
        return e;
    }

    throw std::runtime_error("Unexpected token in expression");
}
