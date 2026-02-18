// parser - handles c-style and english syntaxes

export module opus.parser;

import opus.types;
import opus.lexer;
import opus.ast;
import std;

export namespace opus {

struct ParseError {
    std::string message;
    SourceLoc loc;
    
    std::string to_string() const {
        return std::format("{}:{}:{}: error: {}", 
            loc.file, loc.line, loc.column, message);
    }
};

class Parser {
public:
    Parser(std::vector<Token> tokens, SyntaxMode mode)
        : tokens_(std::move(tokens))
        , mode_(mode)
        , pos_(0)
        , loop_counter_(0)
        , max_iterations_(100000)  // Safeguard: max iterations before abort
    {}

    // Parse entire module
    std::expected<ast::Module, std::vector<ParseError>> parse_module(std::string_view name) {
        ast::Module mod;
        mod.name = std::string(name);
        mod.syntax = mode_;

        while (!at_end() && !check(TokenKind::Eof)) {
            if (!check_loop_safeguard("parse_module")) {
                break;
            }
            auto decl = parse_decl();
            if (!decl) {
                // Error recovery: skip to next statement
                synchronize();
            } else {
                mod.decls.push_back(std::move(*decl));
            }
        }

        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }
        return mod;
    }

private:
    std::vector<Token> tokens_;
    SyntaxMode mode_;
    std::size_t pos_;
    std::vector<ParseError> errors_;
    std::size_t loop_counter_;
    std::size_t max_iterations_;
    bool safeguard_triggered_ = false;

    // ========================================================================
    // SAFEGUARDS
    // ========================================================================
    
    bool check_loop_safeguard(const char* location) {
        if (safeguard_triggered_) return false;
        loop_counter_++;
        if (loop_counter_ > max_iterations_) {
            safeguard_triggered_ = true;
            std::string msg = std::format(
                "parser stuck at token '{}' (pos {}) in {} - possible syntax error",
                at_end() ? "EOF" : std::string(current().text), pos_, location);
            std::cerr << "\n[OPUS ERROR] " << msg << "\n";
            error(msg);
            return false;
        }
        return true;
    }
    
    void reset_loop_counter() {
        loop_counter_ = 0;
    }

    // ========================================================================
    // TOKEN MANIPULATION
    // ========================================================================

    bool at_end() const { return pos_ >= tokens_.size(); }
    
    const Token& peek(std::size_t offset = 0) const {
        if (pos_ + offset >= tokens_.size()) {
            return tokens_.back(); // EOF token
        }
        return tokens_[pos_ + offset];
    }

    const Token& current() const { return peek(0); }
    const Token& previous() const { return tokens_[pos_ - 1]; }

    Token advance() {
        if (!at_end()) pos_++;
        return previous();
    }

    bool check(TokenKind kind) const {
        return !at_end() && current().kind == kind;
    }

    bool match(TokenKind kind) {
        if (check(kind)) {
            advance();
            return true;
        }
        return false;
    }

    template<typename... Kinds>
    bool match_any(Kinds... kinds) {
        return (match(kinds) || ...);
    }

    Token consume(TokenKind kind, std::string_view message) {
        if (check(kind)) return advance();
        error(message);
        return current();
    }

    void error(std::string_view message) {
        errors_.push_back(ParseError{
            .message = std::string(message),
            .loc = current().loc
        });
    }

    void synchronize() {
        advance();
        std::size_t sync_counter = 0;
        while (!at_end()) {
            if (++sync_counter > 10000) {
                std::cerr << "[OPUS ERROR] Synchronize exceeded 10000 iterations, aborting\n";
                return;
            }
            if (previous().kind == TokenKind::Semicolon) return;
            
            switch (current().kind) {
                // v2.0 keywords
                case TokenKind::Function:
                case TokenKind::Class:
                case TokenKind::Struct:
                case TokenKind::Var:
                case TokenKind::Let:
                case TokenKind::Const:
                case TokenKind::Return:
                case TokenKind::If:
                case TokenKind::While:
                case TokenKind::For:
                case TokenKind::Thread:
                case TokenKind::Unsafe:
                // Legacy keywords
                case TokenKind::Fn:
                case TokenKind::Enum:
                case TokenKind::Project:
                // English keywords
                case TokenKind::Define:
                case TokenKind::Create:
                // Type keywords (C++ style declarations)
                case TokenKind::TypeInt:
                case TokenKind::TypeLong:
                case TokenKind::TypeVoid:
                case TokenKind::TypeBool:
                case TokenKind::TypeFloat:
                case TokenKind::TypeDouble:
                    return;
                default:
                    advance();
            }
        }
    }

    // ========================================================================
    // DECLARATIONS
    // ========================================================================

    std::optional<ast::DeclPtr> parse_decl() {
        // English syntax: define, create
        if (check(TokenKind::Define) || check(TokenKind::Create)) {
            return parse_decl_english();
        }
        
        // C-style (default): fn, struct, const, import, export, extern
        return parse_decl_c();
    }

    // C-Style v2.0: function RetType name(params) { body }
    // Or: RetType name(params) { body } (no function keyword)
    // Or: class/struct/const/import/extern
    std::optional<ast::DeclPtr> parse_decl_c() {
        // v2.0: 'function' keyword style: function int add(int a, int b) { ... }
        if (match(TokenKind::Function)) {
            return parse_fn_decl_v2();
        }
        // v2.0: 'class' keyword
        if (match(TokenKind::Class)) {
            return parse_class_decl();
        }
        // Legacy: 'fn' keyword (backward compat)
        if (match(TokenKind::Fn)) {
            return parse_fn_decl_c();
        }
        if (match(TokenKind::Struct)) {
            return parse_struct_decl();
        }
        if (match(TokenKind::Enum)) {
            return parse_enum_decl();
        }
        if (match(TokenKind::Const)) {
            return parse_const_decl();
        }
        if (match(TokenKind::Import)) {
            return parse_import_decl();
        }
        if (match(TokenKind::Extern)) {
            return parse_extern_decl();
        }
        // v2.0: project declaration
        if (match(TokenKind::Project)) {
            return parse_project_decl();
        }
        // global variable: let x = 10 or var y = 20
        if (match(TokenKind::Let)) {
            return parse_static_decl(false);
        }
        if (match(TokenKind::Var)) {
            return parse_static_decl(true);
        }
        // v2.0: C++ style type-first declaration (int main() { ... })
        if (current().is_type()) {
            return parse_fn_decl_v2_type_first();
        }
        
        error("expected declaration (function, class, struct, etc.)");
        return std::nullopt;
    }

    // English: define function name returning Type ... end function
    std::optional<ast::DeclPtr> parse_decl_english() {
        if (match(TokenKind::Define)) {
            if (match(TokenKind::Function)) {
                return parse_fn_decl_english();
            }
            if (match(TokenKind::Variable)) {
                // global variable
            }
        }
        if (match(TokenKind::Create)) {
            if (match(TokenKind::Struct)) {
                return parse_struct_decl();
            }
        }
        
        error("expected declaration (define function, create struct, etc.)");
        return std::nullopt;
    }

    // ========================================================================
    // FUNCTION DECLARATIONS
    // ========================================================================

    std::optional<ast::DeclPtr> parse_fn_decl_c() {
        SourceSpan span{.start = previous().loc};
        
        Token name_tok = consume(TokenKind::Ident, "expected function name");
        std::string name(name_tok.text);

        consume(TokenKind::LParen, "expected '(' after function name");
        auto params = parse_param_list();
        consume(TokenKind::RParen, "expected ')' after parameters");

        // Return type
        Type ret_type = Type::make_primitive(PrimitiveType::Void);
        if (match(TokenKind::Arrow)) {
            auto t = parse_type();
            if (t) ret_type = std::move(*t);
        }

        // Body (use mixed parsing to allow any syntax inside)
        consume(TokenKind::LBrace, "expected '{' before function body");
        auto body = parse_block_stmts_mixed();
        consume(TokenKind::RBrace, "expected '}' after function body");

        span.end = previous().loc;

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::FnDecl{
            .name = std::move(name),
            .params = std::move(params),
            .return_type = std::move(ret_type),
            .body = std::move(body),
        };
        decl->span = span;
        return decl;
    }

    std::optional<ast::DeclPtr> parse_fn_decl_english() {
        SourceSpan span{.start = previous().loc};
        
        Token name_tok = consume(TokenKind::Ident, "expected function name");
        std::string name(name_tok.text);

        // Parameters (optional)
        std::vector<ast::Param> params;
        if (match(TokenKind::With)) {
            // with parameter x as i32, y as i32
            do {
                Token pname = consume(TokenKind::Ident, "expected parameter name");
                consume(TokenKind::As, "expected 'as' after parameter name");
                auto ptype = parse_type();
                if (ptype) {
                    params.push_back(ast::Param{
                        .name = std::string(pname.text),
                        .type = std::move(*ptype)
                    });
                }
            } while (match(TokenKind::Comma));
        }

        // Return type
        Type ret_type = Type::make_primitive(PrimitiveType::Void);
        if (match(TokenKind::Returning)) {
            auto t = parse_type();
            if (t) ret_type = std::move(*t);
        }

        // Body: begin ... end function
        // Or just statements until "end function"
        std::vector<ast::StmtPtr> body;
        while (!at_end() && !check(TokenKind::End)) {
            auto stmt = parse_stmt();
            if (stmt) body.push_back(std::move(*stmt));
        }
        
        consume(TokenKind::End, "expected 'end'");
        consume(TokenKind::Function, "expected 'function' after 'end'");

        span.end = previous().loc;

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::FnDecl{
            .name = std::move(name),
            .params = std::move(params),
            .return_type = std::move(ret_type),
            .body = std::move(body),
        };
        decl->span = span;
        return decl;
    }

    // ========================================================================
    // v2.0 FUNCTION DECLARATIONS - C++/JS Hybrid Style
    // ========================================================================

    // v2.0: function RetType name(Type param, Type param) { body }
    std::optional<ast::DeclPtr> parse_fn_decl_v2() {
        SourceSpan span{.start = previous().loc};
        
        // Check for 'thread' modifier
        bool is_thread = match(TokenKind::Thread);
        
        // Return type comes first (C++ style)
        auto ret_type_opt = parse_type();
        Type ret_type = ret_type_opt ? std::move(*ret_type_opt) : Type::make_primitive(PrimitiveType::Void);
        
        // Function name (allow type keywords as names too, e.g. "double")
        Token name_tok;
        if (check(TokenKind::Ident)) {
            name_tok = advance();
        } else if (current().is_type()) {
            // Allow type keywords as function names
            name_tok = advance();
        } else {
            error("expected function name");
            name_tok = current();
        }
        std::string name(name_tok.text);

        // Parameters: (Type name, Type name, ...)
        consume(TokenKind::LParen, "expected '(' after function name");
        auto params = parse_param_list_v2();
        consume(TokenKind::RParen, "expected ')' after parameters");

        // Body { ... }
        consume(TokenKind::LBrace, "expected '{' before function body");
        auto body = parse_block_stmts_v2();
        consume(TokenKind::RBrace, "expected '}' after function body");

        span.end = previous().loc;

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::FnDecl{
            .name = std::move(name),
            .params = std::move(params),
            .return_type = std::move(ret_type),
            .body = std::move(body),
        };
        decl->span = span;
        return decl;
    }

    // v2.0: Type name(Type param, ...) { body } - no 'function' keyword
    std::optional<ast::DeclPtr> parse_fn_decl_v2_type_first() {
        SourceSpan span{.start = current().loc};
        
        // Return type is current token (already verified to be a type)
        auto ret_type_opt = parse_type();
        Type ret_type = ret_type_opt ? std::move(*ret_type_opt) : Type::make_primitive(PrimitiveType::Void);
        
        // Function name
        Token name_tok = consume(TokenKind::Ident, "expected function name after type");
        std::string name(name_tok.text);

        // Parameters
        consume(TokenKind::LParen, "expected '('");
        auto params = parse_param_list_v2();
        consume(TokenKind::RParen, "expected ')'");

        // Body
        consume(TokenKind::LBrace, "expected '{'");
        auto body = parse_block_stmts_v2();
        consume(TokenKind::RBrace, "expected '}'");

        span.end = previous().loc;

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::FnDecl{
            .name = std::move(name),
            .params = std::move(params),
            .return_type = std::move(ret_type),
            .body = std::move(body),
        };
        decl->span = span;
        return decl;
    }

    // v2.0 param list: Type name, Type name, ...
    std::vector<ast::Param> parse_param_list_v2() {
        std::vector<ast::Param> params;
        if (check(TokenKind::RParen)) return params;

        do {
            // Type comes first (C++ style)
            auto ptype = parse_type();
            if (!ptype) {
                error("expected parameter type");
                return params;
            }
            Token pname = consume(TokenKind::Ident, "expected parameter name after type");
            params.push_back(ast::Param{
                .name = std::string(pname.text),
                .type = std::move(*ptype),
                .is_mut = true  // All params are mutable by default in v2.0
            });
        } while (match(TokenKind::Comma));

        return params;
    }

    // v2.0 block statements with mixed syntax support
    std::vector<ast::StmtPtr> parse_block_stmts_v2() {
        std::vector<ast::StmtPtr> stmts;
        while (!check(TokenKind::RBrace) && !at_end()) {
            if (!check_loop_safeguard("parse_block_stmts_v2")) break;
            
            std::size_t before = pos_;
            auto stmt = parse_stmt_v2();
            if (stmt) {
                stmts.push_back(std::move(*stmt));
            } else {
                // If parse failed and position didn't move, force advance to prevent loop
                if (pos_ == before && !at_end()) {
                    advance();
                }
            }
        }
        return stmts;
    }

    // v2.0 statement parsing
    std::optional<ast::StmtPtr> parse_stmt_v2() {
        // capture location before parsing for debug line mapping
        SourceLoc stmt_start = current().loc;
        
        // v2.0: let x = 10;
        if (match(TokenKind::Let)) {
            auto s = parse_let_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // v2.0: var x = 10; (mutable)
        if (match(TokenKind::Var)) {
            auto s = parse_var_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // v2.0: auto x = 10; (C++ style, mutable with inference)
        if (match(TokenKind::Auto)) {
            auto s = parse_var_stmt_v2();  // Same as var - mutable with type inference
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // v2.0: const x = 10;
        if (match(TokenKind::Const)) {
            auto s = parse_const_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // v2.0: Type x = 10; (C++ style variable declaration)
        if (current().is_type()) {
            auto s = parse_type_decl_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // return
        if (match(TokenKind::Return)) {
            auto s = parse_return_stmt();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // v2.0: if without parentheses
        if (match(TokenKind::If)) {
            auto s = parse_if_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // v2.0: while without parentheses
        if (match(TokenKind::While)) {
            auto s = parse_while_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // parallel for - splits work across cores
        if (match(TokenKind::Parallel)) {
            auto s = parse_parallel_for();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // for loop
        if (match(TokenKind::For)) {
            auto s = parse_for_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Loop)) {
            auto s = parse_loop_stmt();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Break)) {
            match(TokenKind::Semicolon);
            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::BreakStmt{};
            stmt->span.start = stmt_start;
            return stmt;
        }
        if (match(TokenKind::Continue)) {
            match(TokenKind::Semicolon);
            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::ContinueStmt{};
            stmt->span.start = stmt_start;
            return stmt;
        }
        // Block
        if (match(TokenKind::LBrace)) {
            auto stmts = parse_block_stmts_v2();
            consume(TokenKind::RBrace, "expected '}'");
            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::BlockStmt{.stmts = std::move(stmts)};
            stmt->span.start = stmt_start;
            return stmt;
        }

        // Expression statement
        auto expr = parse_expr();
        if (!expr) {
            // Advance past problematic token to prevent infinite loop
            if (!at_end()) advance();
            return std::nullopt;
        }
        match(TokenKind::Semicolon);
        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ExprStmt{.expr = std::move(*expr)};
        stmt->span.start = stmt_start;
        return stmt;
    }

    // v2.0: let x = 10; (immutable)
    std::optional<ast::StmtPtr> parse_let_stmt_v2() {
        // Accept type keywords as variable names (contextual keywords)
        Token name;
        if (check(TokenKind::Ident)) {
            name = advance();
        } else if (current().is_type()) {
            // Type keywords like 'ptr', 'int' can be variable names
            name = advance();
        } else {
            name = consume(TokenKind::Ident, "expected variable name");
        }
        
        std::optional<Type> type;
        // Optional type annotation with colon (for backward compat)
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }

        std::optional<ast::ExprPtr> init;
        if (match(TokenKind::Assign)) {
            init = parse_expr();
        }

        match(TokenKind::Semicolon);

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::LetStmt{
            .name = std::string(name.text),
            .type = std::move(type),
            .init = std::move(init),
            .is_mut = false  // let is immutable
        };
        return stmt;
    }

    // v2.0: var x = 10; (mutable, JS style)
    std::optional<ast::StmtPtr> parse_var_stmt_v2() {
        // Accept type keywords as variable names (contextual keywords)
        Token name;
        if (check(TokenKind::Ident)) {
            name = advance();
        } else if (current().is_type()) {
            name = advance();
        } else {
            name = consume(TokenKind::Ident, "expected variable name");
        }
        
        std::optional<Type> type;
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }

        std::optional<ast::ExprPtr> init;
        if (match(TokenKind::Assign)) {
            init = parse_expr();
        }

        match(TokenKind::Semicolon);

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::LetStmt{
            .name = std::string(name.text),
            .type = std::move(type),
            .init = std::move(init),
            .is_mut = true  // var is mutable
        };
        return stmt;
    }

    // v2.0: const x = 10;
    std::optional<ast::StmtPtr> parse_const_stmt_v2() {
        Token name = consume(TokenKind::Ident, "expected constant name");
        
        std::optional<Type> type;
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }

        consume(TokenKind::Assign, "expected '=' after constant name");
        auto init = parse_expr();

        match(TokenKind::Semicolon);

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::LetStmt{
            .name = std::string(name.text),
            .type = std::move(type),
            .init = std::move(init),
            .is_mut = false
        };
        return stmt;
    }

    // v2.0: Type name = value; (C++ style)
    std::optional<ast::StmtPtr> parse_type_decl_stmt_v2() {
        auto type = parse_type();
        if (!type) return std::nullopt;
        
        Token name = consume(TokenKind::Ident, "expected variable name after type");
        
        std::optional<ast::ExprPtr> init;
        if (match(TokenKind::Assign)) {
            init = parse_expr();
        }

        match(TokenKind::Semicolon);

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::LetStmt{
            .name = std::string(name.text),
            .type = std::move(type),
            .init = std::move(init),
            .is_mut = true  // C++ style is mutable by default
        };
        return stmt;
    }

    // v2.0: if cond { ... } else { ... } - no parentheses!
    std::optional<ast::StmtPtr> parse_if_stmt_v2() {
        auto cond = parse_expr();
        
        consume(TokenKind::LBrace, "expected '{' after if condition");
        auto then_block = parse_block_stmts_v2();
        consume(TokenKind::RBrace, "expected '}'");

        std::optional<std::vector<ast::StmtPtr>> else_block;
        if (match(TokenKind::Else)) {
            // else if
            if (check(TokenKind::If)) {
                advance();
                auto else_if = parse_if_stmt_v2();
                if (else_if) {
                    else_block = std::vector<ast::StmtPtr>{};
                    else_block->push_back(std::move(*else_if));
                }
            } else {
                consume(TokenKind::LBrace, "expected '{'");
                else_block = parse_block_stmts_v2();
                consume(TokenKind::RBrace, "expected '}'");
            }
        }

        if (!cond) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::IfStmt{
            .condition = std::move(*cond),
            .then_block = std::move(then_block),
            .else_block = std::move(else_block)
        };
        return stmt;
    }

    // v2.0: while cond { ... } - no parentheses!
    std::optional<ast::StmtPtr> parse_while_stmt_v2() {
        auto cond = parse_expr();
        consume(TokenKind::LBrace, "expected '{' after while condition");
        auto body = parse_block_stmts_v2();
        consume(TokenKind::RBrace, "expected '}'");

        if (!cond) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::WhileStmt{
            .condition = std::move(*cond),
            .body = std::move(body)
        };
        return stmt;
    }

    // v2.0: for i in range(0, 10) { ... }
    std::optional<ast::StmtPtr> parse_for_stmt_v2() {
        Token var = consume(TokenKind::Ident, "expected variable");
        consume(TokenKind::In, "expected 'in'");
        auto iter = parse_expr();
        consume(TokenKind::LBrace, "expected '{'");
        auto body = parse_block_stmts_v2();
        consume(TokenKind::RBrace, "expected '}'");

        if (!iter) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ForStmt{
            .var_name = std::string(var.text),
            .iterable = std::move(*iter),
            .body = std::move(body)
        };
        return stmt;
    }

    // v2.0: class Name { ... }
    std::optional<ast::DeclPtr> parse_class_decl() {
        Token name_tok = consume(TokenKind::Ident, "expected class name");
        consume(TokenKind::LBrace, "expected '{'");

        std::vector<std::pair<std::string, Type>> fields;
        std::vector<ast::MethodDecl> methods;

        while (!check(TokenKind::RBrace) && !at_end()) {
            if (!check_loop_safeguard("parse_class_decl")) break;
            
            // Method: function RetType name(...) { } or function void name(self, ...) { }
            if (match(TokenKind::Function)) {
                // Parse method similar to parse_fn_decl_v2
                auto ret_type = parse_type();
                Token method_name = consume(TokenKind::Ident, "expected method name");
                consume(TokenKind::LParen, "expected '(' after method name");
                auto params = parse_param_list_v2();
                consume(TokenKind::RParen, "expected ')'");
                consume(TokenKind::LBrace, "expected '{'");
                auto body = parse_block_stmts_v2();
                consume(TokenKind::RBrace, "expected '}'");
                
                if (ret_type) {
                    methods.push_back(ast::MethodDecl{
                        .name = std::string(method_name.text),
                        .params = std::move(params),
                        .return_type = std::move(*ret_type),
                        .body = std::move(body),
                        .is_static = false
                    });
                }
            }
            // Field with type annotation: name: Type,
            else if (check(TokenKind::Ident)) {
                Token fname = advance();
                if (match(TokenKind::Colon)) {
                    // Rust-style: name: Type
                    auto ftype = parse_type();
                    if (ftype) {
                        fields.emplace_back(std::string(fname.text), std::move(*ftype));
                    }
                    match(TokenKind::Comma);
                    match(TokenKind::Semicolon);
                } else {
                    error("expected ':' after field name");
                }
            }
            // C-style field: Type name;
            else if (current().is_type()) {
                auto ftype = parse_type();
                Token fname = consume(TokenKind::Ident, "expected field name");
                if (ftype) {
                    fields.emplace_back(std::string(fname.text), std::move(*ftype));
                }
                match(TokenKind::Semicolon);
                match(TokenKind::Comma);
            }
            else {
                error("expected field or method in class");
                advance();
            }
        }

        consume(TokenKind::RBrace, "expected '}'");

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::ClassDecl{
            .name = std::string(name_tok.text),
            .fields = std::move(fields),
            .methods = std::move(methods),
            .parent = std::nullopt
        };
        return decl;
    }
    
    std::vector<ast::Param> parse_param_list() {
        std::vector<ast::Param> params;
        if (check(TokenKind::RParen)) return params;

        do {
            bool is_mut = match(TokenKind::Mut);
            Token pname = consume(TokenKind::Ident, "expected parameter name");
            consume(TokenKind::Colon, "expected ':' after parameter name");
            auto ptype = parse_type();
            if (ptype) {
                params.push_back(ast::Param{
                    .name = std::string(pname.text),
                    .type = std::move(*ptype),
                    .is_mut = is_mut
                });
            }
        } while (match(TokenKind::Comma));

        return params;
    }

    // ========================================================================
    // OTHER DECLARATIONS
    // ========================================================================

    std::optional<ast::DeclPtr> parse_struct_decl() {
        Token name_tok = consume(TokenKind::Ident, "expected struct name");
        consume(TokenKind::LBrace, "expected '{'");

        std::vector<std::pair<std::string, Type>> fields;
        while (!check(TokenKind::RBrace) && !at_end()) {
            if (!check_loop_safeguard("parse_struct_decl")) {
                break;
            }
            // Support BOTH syntaxes:
            // 1. Rust-style: name: Type,
            // 2. C-style: Type name;
            
            if (current().is_type()) {
                // C-style: Type name;
                auto ftype = parse_type();
                Token fname = consume(TokenKind::Ident, "expected field name");
                if (ftype) {
                    fields.emplace_back(std::string(fname.text), std::move(*ftype));
                }
                match(TokenKind::Semicolon);  // C-style uses semicolon
                match(TokenKind::Comma);      // But also allow comma
            } else {
                // Rust-style: name: Type,
                Token fname = consume(TokenKind::Ident, "expected field name");
                consume(TokenKind::Colon, "expected ':'");
                auto ftype = parse_type();
                if (ftype) {
                    fields.emplace_back(std::string(fname.text), std::move(*ftype));
                }
                match(TokenKind::Comma);      // Optional trailing comma
                match(TokenKind::Semicolon);  // But also allow semicolon
            }
        }

        consume(TokenKind::RBrace, "expected '}'");

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::StructDecl{
            .name = std::string(name_tok.text),
            .fields = std::move(fields)
        };
        return decl;
    }

    // enum Name { Variant, Variant2 = 5 }
    std::optional<ast::DeclPtr> parse_enum_decl() {
        Token name_tok = consume(TokenKind::Ident, "expected enum name");
        consume(TokenKind::LBrace, "expected '{' after enum name");
        
        std::vector<std::pair<std::string, std::optional<std::int64_t>>> variants;
        std::int64_t next_value = 0;
        
        while (!check(TokenKind::RBrace)) {
            Token variant_name = consume(TokenKind::Ident, "expected variant name");
            
            std::optional<std::int64_t> explicit_value;
            if (match(TokenKind::Assign)) {
                Token val_tok = consume(TokenKind::IntLit, "expected integer value");
                explicit_value = std::get<std::int64_t>(val_tok.value);
                next_value = *explicit_value + 1;
            } else {
                explicit_value = next_value++;
            }
            
            variants.emplace_back(std::string(variant_name.text), explicit_value);
            
            if (!check(TokenKind::RBrace)) {
                if (!match(TokenKind::Comma)) {
                    if (!check(TokenKind::RBrace)) {
                        error("expected ',' or '}' after enum variant");
                        return std::nullopt;
                    }
                }
            }
        }
        
        consume(TokenKind::RBrace, "expected '}' after enum variants");
        
        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::EnumDecl{
            .name = std::string(name_tok.text),
            .variants = std::move(variants)
        };
        return decl;
    }

    std::optional<ast::DeclPtr> parse_const_decl() {
        Token name_tok = consume(TokenKind::Ident, "expected constant name");
        consume(TokenKind::Colon, "expected ':'");
        auto type = parse_type();
        consume(TokenKind::Assign, "expected '='");
        auto value = parse_expr();

        if (!type || !value) return std::nullopt;

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::ConstDecl{
            .name = std::string(name_tok.text),
            .type = std::move(*type),
            .value = std::move(*value)
        };
        match(TokenKind::Semicolon);
        return decl;
    }

    // let x = 10 or var y: int = 20
    std::optional<ast::DeclPtr> parse_static_decl(bool is_mut) {
        Token name_tok = consume(TokenKind::Ident, "expected variable name");
        
        std::optional<Type> type;
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }
        
        std::optional<ast::ExprPtr> init;
        if (match(TokenKind::Assign)) {
            auto expr = parse_expr();
            if (expr) {
                init = std::move(*expr);
            }
        }
        
        match(TokenKind::Semicolon);
        
        auto decl = std::make_unique<ast::Decl>();
        ast::StaticDecl sd;
        sd.name = std::string(name_tok.text);
        sd.type = type ? std::move(*type) : Type::make_primitive(PrimitiveType::I64);
        sd.init = std::move(init);
        sd.is_mut = is_mut;
        decl->kind = std::move(sd);
        return decl;
    }

    std::optional<ast::DeclPtr> parse_import_decl() {
        Token path_tok = consume(TokenKind::Ident, "expected module path");
        std::string path(path_tok.text);
        
        while (match(TokenKind::Dot)) {
            Token next = consume(TokenKind::Ident, "expected identifier");
            path += ".";
            path += next.text;
        }

        std::optional<std::string> alias;
        if (match(TokenKind::As)) {
            Token alias_tok = consume(TokenKind::Ident, "expected alias");
            alias = std::string(alias_tok.text);
        }

        match(TokenKind::Semicolon);

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::ImportDecl{
            .path = std::move(path),
            .alias = std::move(alias)
        };
        return decl;
    }

    // project Name { entry: "main.op", output: "app.dll", ... }
    std::optional<ast::DeclPtr> parse_project_decl() {
        Token name_tok = consume(TokenKind::Ident, "expected project name");
        consume(TokenKind::LBrace, "expected '{' after project name");
        
        ast::ProjectDecl proj;
        proj.name = std::string(name_tok.text);
        proj.mode = "dll";  // Default mode
        
        while (!check(TokenKind::RBrace) && !at_end()) {
            if (!check_loop_safeguard("parse_project_decl")) break;
            
            // get property name - could be ident or keyword like 'include'
            std::string key_str;
            if (check(TokenKind::Include)) {
                advance();
                key_str = "include";
            } else if (check(TokenKind::Ident)) {
                key_str = std::string(advance().text);
            } else {
                error("expected project property name");
                advance();
                continue;
            }
            
            consume(TokenKind::Colon, "expected ':' after property name");
            
            if (key_str == "entry") {
                Token val = consume(TokenKind::StringLit, "expected string for entry");
                proj.entry = std::get<std::string>(val.value);
            } else if (key_str == "output") {
                Token val = consume(TokenKind::StringLit, "expected string for output");
                proj.output = std::get<std::string>(val.value);
            } else if (key_str == "mode") {
                // mode can be ident like 'dll' or 'exe'
                if (check(TokenKind::Ident)) {
                    proj.mode = std::string(advance().text);
                } else {
                    error("expected 'dll' or 'exe' for mode");
                }
            } else if (key_str == "include") {
                consume(TokenKind::LBracket, "expected '[' for include list");
                while (!check(TokenKind::RBracket) && !at_end()) {
                    if (!check_loop_safeguard("parse_project_include")) break;
                    Token path = consume(TokenKind::StringLit, "expected include path string");
                    proj.includes.push_back(std::get<std::string>(path.value));
                    if (!check(TokenKind::RBracket)) {
                        match(TokenKind::Comma);
                    }
                }
                consume(TokenKind::RBracket, "expected ']' after include list");
            } else if (key_str == "debug") {
                // debug: true or debug: false
                if (check(TokenKind::True)) {
                    advance();
                    proj.debug = true;
                } else if (check(TokenKind::False)) {
                    advance();
                    proj.debug = false;
                } else if (check(TokenKind::Ident)) {
                    auto val = std::string(advance().text);
                    proj.debug = (val == "true");
                } else {
                    error("expected 'true' or 'false' for debug");
                }
            } else if (key_str == "healing") {
                // healing: auto | freeze | off
                if (check(TokenKind::Ident)) {
                    proj.healing = std::string(advance().text);
                } else if (check(TokenKind::StringLit)) {
                    proj.healing = std::get<std::string>(advance().value);
                } else {
                    error("expected 'auto', 'freeze', or 'off' for healing");
                }
            } else {
                error(std::format("unknown project property: '{}'", key_str));
                advance();
            }
            
            match(TokenKind::Comma);
        }
        
        consume(TokenKind::RBrace, "expected '}' after project properties");
        
        auto decl = std::make_unique<ast::Decl>();
        decl->kind = std::move(proj);
        return decl;
    }

    std::optional<ast::DeclPtr> parse_extern_decl() {
        if (match(TokenKind::Fn)) {
            // extern fn name(params) -> RetType
            Token name_tok = consume(TokenKind::Ident, "expected function name");
            consume(TokenKind::LParen, "expected '('");
            auto params = parse_param_list();
            consume(TokenKind::RParen, "expected ')'");

            Type ret_type = Type::make_primitive(PrimitiveType::Void);
            if (match(TokenKind::Arrow) || match(TokenKind::Colon)) {
                auto t = parse_type();
                if (t) ret_type = std::move(*t);
            }

            match(TokenKind::Semicolon);

            auto decl = std::make_unique<ast::Decl>();
            decl->kind = ast::FnDecl{
                .name = std::string(name_tok.text),
                .params = std::move(params),
                .return_type = std::move(ret_type),
                .body = {},
                .is_extern = true
            };
            return decl;
        }
        error("expected 'fn' after 'extern'");
        return std::nullopt;
    }

    // ========================================================================
    // STATEMENTS
    // ========================================================================

    std::optional<ast::StmtPtr> parse_stmt() {
        switch (mode_) {
            case SyntaxMode::CStyle:  return parse_stmt_c();
            case SyntaxMode::English: return parse_stmt_english();
        }
        return std::nullopt;
    }

    std::vector<ast::StmtPtr> parse_block_stmts() {
        std::vector<ast::StmtPtr> stmts;
        while (!check(TokenKind::RBrace) && !at_end()) {
            if (!check_loop_safeguard("parse_block_stmts")) break;
            
            std::size_t before = pos_;
            auto stmt = parse_stmt_c();
            if (stmt) {
                stmts.push_back(std::move(*stmt));
            } else if (pos_ == before && !at_end()) {
                advance();  // Prevent infinite loop
            }
        }
        return stmts;
    }

    
    // Mixed mode: detect syntax per-statement
    std::vector<ast::StmtPtr> parse_block_stmts_mixed() {
        std::vector<ast::StmtPtr> stmts;
        while (!check(TokenKind::RBrace) && !check(TokenKind::End) && !at_end()) {
            if (!check_loop_safeguard("parse_block_stmts_mixed")) break;
            
            std::size_t before = pos_;
            auto stmt = parse_stmt_mixed();
            if (stmt) {
                stmts.push_back(std::move(*stmt));
            } else if (pos_ == before && !at_end()) {
                advance();
            }
        }
        return stmts;
    }
    
    // Detect which syntax this statement uses and parse accordingly
    std::optional<ast::StmtPtr> parse_stmt_mixed() {
        // English syntax: create variable, set ... to, etc.
        if (check(TokenKind::Create)) return parse_stmt_english();
        if (check(TokenKind::Set)) return parse_stmt_english();
        
        // C-style (default)
        return parse_stmt_c();
    }

    std::optional<ast::StmtPtr> parse_stmt_c() {
        SourceLoc stmt_start = current().loc;
        
        if (match(TokenKind::Let)) {
            auto s = parse_let_stmt();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Return)) {
            auto s = parse_return_stmt();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::If)) {
            auto s = parse_if_stmt_c();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::While)) {
            auto s = parse_while_stmt_c();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Parallel)) {
            auto s = parse_parallel_for();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::For)) {
            auto s = parse_for_stmt_c();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Loop)) {
            auto s = parse_loop_stmt();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Break)) {
            match(TokenKind::Semicolon);
            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::BreakStmt{};
            stmt->span.start = stmt_start;
            return stmt;
        }
        if (match(TokenKind::Continue)) {
            match(TokenKind::Semicolon);
            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::ContinueStmt{};
            stmt->span.start = stmt_start;
            return stmt;
        }
        if (match(TokenKind::LBrace)) {
            auto stmts = parse_block_stmts();
            consume(TokenKind::RBrace, "expected '}'");
            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::BlockStmt{.stmts = std::move(stmts)};
            stmt->span.start = stmt_start;
            return stmt;
        }

        // Expression statement
        auto expr = parse_expr();
        if (!expr) return std::nullopt;
        match(TokenKind::Semicolon);
        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ExprStmt{.expr = std::move(*expr)};
        stmt->span.start = stmt_start;
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_stmt_english() {
        if (match(TokenKind::Create) && match(TokenKind::Variable)) {
            // create variable x as i32 with value 0
            Token name = consume(TokenKind::Ident, "expected variable name");
            consume(TokenKind::As, "expected 'as'");
            auto type = parse_type();
            
            std::optional<ast::ExprPtr> init;
            if (match(TokenKind::With) && match(TokenKind::Value)) {
                init = parse_expr();
            }

            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::LetStmt{
                .name = std::string(name.text),
                .type = type ? std::make_optional(std::move(*type)) : std::nullopt,
                .init = std::move(init)
            };
            return stmt;
        }
        if (match(TokenKind::Set)) {
            // set x to 42
            auto target = parse_expr();
            consume(TokenKind::To, "expected 'to'");
            auto value = parse_expr();
            
            if (target && value) {
                auto assign = std::make_unique<ast::Expr>();
                assign->kind = ast::BinaryExpr{
                    .op = ast::BinaryExpr::Op::Assign,
                    .lhs = std::move(*target),
                    .rhs = std::move(*value)
                };
                auto stmt = std::make_unique<ast::Stmt>();
                stmt->kind = ast::ExprStmt{.expr = std::move(assign)};
                return stmt;
            }
        }
        if (match(TokenKind::Return)) {
            return parse_return_stmt();
        }
        if (match(TokenKind::If)) {
            return parse_if_stmt_english();
        }
        if (match(TokenKind::While)) {
            return parse_while_stmt_english();
        }
        if (match(TokenKind::Call)) {
            auto expr = parse_expr();
            if (expr) {
                auto stmt = std::make_unique<ast::Stmt>();
                stmt->kind = ast::ExprStmt{.expr = std::move(*expr)};
                return stmt;
            }
        }

        // Expression statement
        auto expr = parse_expr();
        if (!expr) return std::nullopt;
        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ExprStmt{.expr = std::move(*expr)};
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_let_stmt() {
        bool is_mut = match(TokenKind::Mut);
        Token name = consume(TokenKind::Ident, "expected variable name");
        
        std::optional<Type> type;
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }

        std::optional<ast::ExprPtr> init;
        if (match(TokenKind::Assign)) {
            init = parse_expr();
        }

        match(TokenKind::Semicolon);

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::LetStmt{
            .name = std::string(name.text),
            .type = std::move(type),
            .init = std::move(init),
            .is_mut = is_mut
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_return_stmt() {
        std::optional<ast::ExprPtr> value;
        if (!check(TokenKind::Semicolon) && !check(TokenKind::RBrace) && !at_end()) {
            value = parse_expr();
        }
        match(TokenKind::Semicolon);

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ReturnStmt{.value = std::move(value)};
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_if_stmt_c() {
        consume(TokenKind::LParen, "expected '(' after 'if'");
        auto cond = parse_expr();
        consume(TokenKind::RParen, "expected ')'");
        
        consume(TokenKind::LBrace, "expected '{'");
        auto then_block = parse_block_stmts();
        consume(TokenKind::RBrace, "expected '}'");

        std::optional<std::vector<ast::StmtPtr>> else_block;
        if (match(TokenKind::Else)) {
            consume(TokenKind::LBrace, "expected '{'");
            else_block = parse_block_stmts();
            consume(TokenKind::RBrace, "expected '}'");
        }

        if (!cond) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::IfStmt{
            .condition = std::move(*cond),
            .then_block = std::move(then_block),
            .else_block = std::move(else_block)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_if_stmt_english() {
        auto cond = parse_expr();
        consume(TokenKind::Then, "expected 'then'");
        
        std::vector<ast::StmtPtr> then_block;
        while (!check(TokenKind::Else) && !check(TokenKind::End) && !at_end()) {
            auto s = parse_stmt();
            if (s) then_block.push_back(std::move(*s));
        }

        std::optional<std::vector<ast::StmtPtr>> else_block;
        if (match(TokenKind::Else)) {
            else_block = std::vector<ast::StmtPtr>{};
            while (!check(TokenKind::End) && !at_end()) {
                auto s = parse_stmt();
                if (s) else_block->push_back(std::move(*s));
            }
        }

        consume(TokenKind::End, "expected 'end'");
        match(TokenKind::If);

        if (!cond) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::IfStmt{
            .condition = std::move(*cond),
            .then_block = std::move(then_block),
            .else_block = std::move(else_block)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_while_stmt_c() {
        consume(TokenKind::LParen, "expected '('");
        auto cond = parse_expr();
        consume(TokenKind::RParen, "expected ')'");
        consume(TokenKind::LBrace, "expected '{'");
        auto body = parse_block_stmts();
        consume(TokenKind::RBrace, "expected '}'");

        if (!cond) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::WhileStmt{
            .condition = std::move(*cond),
            .body = std::move(body)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_while_stmt_english() {
        auto cond = parse_expr();
        consume(TokenKind::Do, "expected 'do'");
        
        std::vector<ast::StmtPtr> body;
        while (!check(TokenKind::End) && !at_end()) {
            auto s = parse_stmt();
            if (s) body.push_back(std::move(*s));
        }
        consume(TokenKind::End, "expected 'end'");
        match(TokenKind::While);

        if (!cond) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::WhileStmt{
            .condition = std::move(*cond),
            .body = std::move(body)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_for_stmt_c() {
        consume(TokenKind::LParen, "expected '('");
        Token var = consume(TokenKind::Ident, "expected variable");
        consume(TokenKind::In, "expected 'in'");
        auto iter = parse_expr();
        consume(TokenKind::RParen, "expected ')'");
        consume(TokenKind::LBrace, "expected '{'");
        auto body = parse_block_stmts();
        consume(TokenKind::RBrace, "expected '}'");

        if (!iter) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ForStmt{
            .var_name = std::string(var.text),
            .iterable = std::move(*iter),
            .body = std::move(body)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_loop_stmt() {
        consume(TokenKind::LBrace, "expected '{'");
        auto body = parse_block_stmts();
        consume(TokenKind::RBrace, "expected '}'");

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::LoopStmt{.body = std::move(body)};
        return stmt;
    }

    // ========================================================================
    // TYPES
    // ========================================================================

    std::optional<Type> parse_type() {
        // Pointer types
        if (match(TokenKind::Star)) {
            bool is_mut = match(TokenKind::Mut);
            auto inner = parse_type();
            if (!inner) return std::nullopt;
            return Type::make_ptr(std::move(*inner), is_mut);
        }

        // Array types [T; N] or [T]
        if (match(TokenKind::LBracket)) {
            auto elem = parse_type();
            if (!elem) return std::nullopt;
            
            std::optional<std::size_t> size;
            if (match(TokenKind::Semicolon)) {
                Token n = consume(TokenKind::IntLit, "expected array size");
                if (auto* v = std::get_if<std::int64_t>(&n.value)) {
                    size = static_cast<std::size_t>(*v);
                }
            }
            consume(TokenKind::RBracket, "expected ']'");
            
            Type t;
            t.kind = ArrayType{
                .element = std::make_unique<Type>(std::move(*elem)),
                .size = size
            };
            return t;
        }

        // Primitive types
        if (current().is_type()) {
            Token t = advance();
            PrimitiveType pt;
            switch (t.kind) {
                // C++/JS-friendly types (v2.0)
                case TokenKind::TypeVoid:   pt = PrimitiveType::Void; break;
                case TokenKind::TypeBool:   pt = PrimitiveType::Bool; break;
                case TokenKind::TypeInt:    pt = PrimitiveType::I32; break;
                case TokenKind::TypeLong:   pt = PrimitiveType::I64; break;
                case TokenKind::TypeShort:  pt = PrimitiveType::I16; break;
                case TokenKind::TypeByte:   pt = PrimitiveType::I8; break;
                case TokenKind::TypeUint:   pt = PrimitiveType::U32; break;
                case TokenKind::TypeUlong:  pt = PrimitiveType::U64; break;
                case TokenKind::TypeUshort: pt = PrimitiveType::U16; break;
                case TokenKind::TypeUbyte:  pt = PrimitiveType::U8; break;
                case TokenKind::TypeFloat:  pt = PrimitiveType::F32; break;
                case TokenKind::TypeDouble: pt = PrimitiveType::F64; break;
                case TokenKind::TypeString: pt = PrimitiveType::Str; break;
                case TokenKind::TypeChar:   pt = PrimitiveType::Char; break;
                case TokenKind::TypePtr:    pt = PrimitiveType::Ptr; break;
                // Legacy types (backward compat)
                case TokenKind::TypeI8:     pt = PrimitiveType::I8; break;
                case TokenKind::TypeI16:    pt = PrimitiveType::I16; break;
                case TokenKind::TypeI32:    pt = PrimitiveType::I32; break;
                case TokenKind::TypeI64:    pt = PrimitiveType::I64; break;
                case TokenKind::TypeI128:   pt = PrimitiveType::I128; break;
                case TokenKind::TypeU8:     pt = PrimitiveType::U8; break;
                case TokenKind::TypeU16:    pt = PrimitiveType::U16; break;
                case TokenKind::TypeU32:    pt = PrimitiveType::U32; break;
                case TokenKind::TypeU64:    pt = PrimitiveType::U64; break;
                case TokenKind::TypeU128:   pt = PrimitiveType::U128; break;
                case TokenKind::TypeF32:    pt = PrimitiveType::F32; break;
                case TokenKind::TypeF64:    pt = PrimitiveType::F64; break;
                case TokenKind::TypeStr:    pt = PrimitiveType::Str; break;
                default: return std::nullopt;
            }
            return Type::make_primitive(pt);
        }

        // Named type
        if (check(TokenKind::Ident)) {
            Token name = advance();
            Type t;
            t.kind = std::string(name.text);
            return t;
        }

        error("expected type");
        return std::nullopt;
    }

    // ========================================================================
    // EXPRESSIONS (Pratt parser)
    // ========================================================================

    std::optional<ast::ExprPtr> parse_expr() {
        return parse_expr_prec(0);
    }

    std::optional<ast::ExprPtr> parse_expr_prec(int min_prec) {
        auto lhs = parse_unary();
        if (!lhs) return std::nullopt;

        while (true) {
            auto [op, prec] = get_binary_op(current().kind);
            if (prec < min_prec) break;
            
            advance();
            auto rhs = parse_expr_prec(prec + 1);
            if (!rhs) return std::nullopt;

            auto bin = std::make_unique<ast::Expr>();
            bin->kind = ast::BinaryExpr{
                .op = op,
                .lhs = std::move(*lhs),
                .rhs = std::move(*rhs)
            };
            lhs = std::move(bin);
        }

        return lhs;
    }

    std::pair<ast::BinaryExpr::Op, int> get_binary_op(TokenKind kind) {
        using Op = ast::BinaryExpr::Op;
        switch (kind) {
            case TokenKind::OrOr:    return {Op::Or, 1};
            case TokenKind::AndAnd:  return {Op::And, 2};
            case TokenKind::Or:      return {Op::Or, 1};
            case TokenKind::And:     return {Op::And, 2};
            case TokenKind::Pipe:    return {Op::BitOr, 3};
            case TokenKind::Caret:   return {Op::BitXor, 4};
            case TokenKind::Ampersand: return {Op::BitAnd, 5};
            case TokenKind::Eq:      return {Op::Eq, 6};
            case TokenKind::Ne:      return {Op::Ne, 6};
            case TokenKind::Lt:      return {Op::Lt, 7};
            case TokenKind::Gt:      return {Op::Gt, 7};
            case TokenKind::Le:      return {Op::Le, 7};
            case TokenKind::Ge:      return {Op::Ge, 7};
            case TokenKind::Shl:     return {Op::Shl, 8};
            case TokenKind::Shr:     return {Op::Shr, 8};
            case TokenKind::Plus:    return {Op::Add, 9};
            case TokenKind::Minus:   return {Op::Sub, 9};
            case TokenKind::Star:    return {Op::Mul, 10};
            case TokenKind::Slash:   return {Op::Div, 10};
            case TokenKind::Percent: return {Op::Mod, 10};
            case TokenKind::Assign:  return {Op::Assign, 0};
            case TokenKind::PlusEq:  return {Op::AddAssign, 0};
            case TokenKind::MinusEq: return {Op::SubAssign, 0};
            case TokenKind::StarEq:  return {Op::MulAssign, 0};
            case TokenKind::SlashEq: return {Op::DivAssign, 0};
            default: return {Op::Add, -1};  // Not a binary op
        }
    }

    std::optional<ast::ExprPtr> parse_unary() {
        using Op = ast::UnaryExpr::Op;
        
        // Prefix ++
        if (match(TokenKind::PlusPlus)) {
            auto operand = parse_unary();
            if (!operand) return std::nullopt;
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::UnaryExpr{.op = Op::PreInc, .operand = std::move(*operand)};
            return expr;
        }
        // Prefix --
        if (match(TokenKind::MinusMinus)) {
            auto operand = parse_unary();
            if (!operand) return std::nullopt;
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::UnaryExpr{.op = Op::PreDec, .operand = std::move(*operand)};
            return expr;
        }
        if (match(TokenKind::Minus)) {
            auto operand = parse_unary();
            if (!operand) return std::nullopt;
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::UnaryExpr{.op = Op::Neg, .operand = std::move(*operand)};
            return expr;
        }
        if (match(TokenKind::Bang) || match(TokenKind::Not)) {
            auto operand = parse_unary();
            if (!operand) return std::nullopt;
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::UnaryExpr{.op = Op::Not, .operand = std::move(*operand)};
            return expr;
        }
        if (match(TokenKind::Tilde)) {
            auto operand = parse_unary();
            if (!operand) return std::nullopt;
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::UnaryExpr{.op = Op::BitNot, .operand = std::move(*operand)};
            return expr;
        }
        if (match(TokenKind::Star)) {
            auto operand = parse_unary();
            if (!operand) return std::nullopt;
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::UnaryExpr{.op = Op::Deref, .operand = std::move(*operand)};
            return expr;
        }
        if (match(TokenKind::Ampersand)) {
            bool is_mut = match(TokenKind::Mut);
            auto operand = parse_unary();
            if (!operand) return std::nullopt;
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::UnaryExpr{
                .op = is_mut ? Op::AddrOfMut : Op::AddrOf, 
                .operand = std::move(*operand)
            };
            return expr;
        }

        return parse_postfix();
    }

    std::optional<ast::ExprPtr> parse_postfix() {
        auto expr = parse_primary();
        if (!expr) return std::nullopt;

        while (true) {
            if (match(TokenKind::LParen)) {
                // Function call
                std::vector<ast::ExprPtr> args;
                if (!check(TokenKind::RParen)) {
                    do {
                        auto arg = parse_expr();
                        if (arg) args.push_back(std::move(*arg));
                    } while (match(TokenKind::Comma));
                }
                consume(TokenKind::RParen, "expected ')'");

                auto call = std::make_unique<ast::Expr>();
                call->kind = ast::CallExpr{
                    .callee = std::move(*expr),
                    .args = std::move(args)
                };
                expr = std::move(call);
            }
            else if (match(TokenKind::LBracket)) {
                // Index
                auto index = parse_expr();
                consume(TokenKind::RBracket, "expected ']'");

                if (index) {
                    auto idx = std::make_unique<ast::Expr>();
                    idx->kind = ast::IndexExpr{
                        .base = std::move(*expr),
                        .index = std::move(*index)
                    };
                    expr = std::move(idx);
                }
            }
            else if (match(TokenKind::Dot)) {
                // Field access
                Token field = consume(TokenKind::Ident, "expected field name");
                auto access = std::make_unique<ast::Expr>();
                access->kind = ast::FieldExpr{
                    .base = std::move(*expr),
                    .field = std::string(field.text)
                };
                expr = std::move(access);
            }
            else if (match(TokenKind::As)) {
                // Cast
                auto target = parse_type();
                if (target) {
                    auto cast = std::make_unique<ast::Expr>();
                    cast->kind = ast::CastExpr{
                        .expr = std::move(*expr),
                        .target_type = std::move(*target)
                    };
                    expr = std::move(cast);
                }
            }
            else if (match(TokenKind::PlusPlus)) {
                // Postfix ++
                auto inc = std::make_unique<ast::Expr>();
                inc->kind = ast::UnaryExpr{
                    .op = ast::UnaryExpr::Op::PostInc,
                    .operand = std::move(*expr)
                };
                expr = std::move(inc);
            }
            else if (match(TokenKind::MinusMinus)) {
                // Postfix --
                auto dec = std::make_unique<ast::Expr>();
                dec->kind = ast::UnaryExpr{
                    .op = ast::UnaryExpr::Op::PostDec,
                    .operand = std::move(*expr)
                };
                expr = std::move(dec);
            }
            else {
                break;
            }
        }

        return expr;
    }

    // Parse struct literal: StructName { field: value, field2: value2 }
    std::optional<ast::ExprPtr> parse_struct_literal(const std::string& struct_name) {
        consume(TokenKind::LBrace, "expected '{' after struct name");
        
        std::vector<std::pair<std::string, ast::ExprPtr>> fields;
        
        while (!check(TokenKind::RBrace)) {
            // field: value
            Token field_name = consume(TokenKind::Ident, "expected field name");
            consume(TokenKind::Colon, "expected ':' after field name");
            
            auto value = parse_expr();
            if (!value) {
                error("expected expression for field value");
                return std::nullopt;
            }
            
            fields.emplace_back(std::string(field_name.text), std::move(*value));
            
            // Optional comma
            if (!check(TokenKind::RBrace)) {
                if (!match(TokenKind::Comma)) {
                    // Allow no comma before }
                    if (!check(TokenKind::RBrace)) {
                        error("expected ',' or '}' after field value");
                        return std::nullopt;
                    }
                }
            }
        }
        
        consume(TokenKind::RBrace, "expected '}' after struct literal");
        
        auto expr = std::make_unique<ast::Expr>();
        expr->kind = ast::StructExpr{
            .name = struct_name,
            .fields = std::move(fields)
        };
        return expr;
    }

    // spawn func(arg1, arg2, ...) -> SpawnExpr
    std::optional<ast::ExprPtr> parse_spawn_expr() {
        Token callee_tok = consume(TokenKind::Ident, "expected function name after 'spawn'");
        
        auto callee = std::make_unique<ast::Expr>();
        callee->kind = ast::IdentExpr{.name = std::string(callee_tok.text)};
        
        consume(TokenKind::LParen, "expected '(' after function name in spawn");
        std::vector<ast::ExprPtr> args;
        if (!check(TokenKind::RParen)) {
            do {
                auto arg = parse_expr();
                if (arg) args.push_back(std::move(*arg));
            } while (match(TokenKind::Comma));
        }
        consume(TokenKind::RParen, "expected ')' after spawn arguments");
        
        auto expr = std::make_unique<ast::Expr>();
        expr->kind = ast::SpawnExpr{
            .callee = std::move(callee),
            .args = std::move(args)
        };
        return expr;
    }

    // await handle_expr -> AwaitExpr
    std::optional<ast::ExprPtr> parse_await_expr() {
        auto handle = parse_expr();
        if (!handle) return std::nullopt;
        
        auto expr = std::make_unique<ast::Expr>();
        expr->kind = ast::AwaitExpr{.handle = std::move(*handle)};
        return expr;
    }

    // parallel for i in range(start, end) { body } -> ParallelForStmt
    std::optional<ast::StmtPtr> parse_parallel_for() {
        consume(TokenKind::For, "expected 'for' after 'parallel'");
        Token var = consume(TokenKind::Ident, "expected loop variable");
        consume(TokenKind::In, "expected 'in'");
        
        // expect range(start, end)
        Token range_tok = consume(TokenKind::Ident, "expected 'range'");
        if (range_tok.text != "range") {
            error("parallel for requires range(start, end)");
            return std::nullopt;
        }
        
        consume(TokenKind::LParen, "expected '(' after 'range'");
        auto start = parse_expr();
        consume(TokenKind::Comma, "expected ',' between range arguments");
        auto end = parse_expr();
        consume(TokenKind::RParen, "expected ')' after range arguments");
        
        consume(TokenKind::LBrace, "expected '{'");
        auto body = parse_block_stmts_v2();
        consume(TokenKind::RBrace, "expected '}'");
        
        if (!start || !end) return std::nullopt;
        
        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ParallelForStmt{
            .var_name = std::string(var.text),
            .start = std::move(*start),
            .end = std::move(*end),
            .body = std::move(body)
        };
        return stmt;
    }

    // atomic_add(ptr, val), atomic_cas(ptr, expected, desired), etc -> AtomicOpExpr
    std::optional<ast::ExprPtr> parse_atomic_op(const std::string& name) {
        using Op = ast::AtomicOpExpr::Op;
        
        Op op;
        int expected_args;
        if (name == "atomic_add") { op = Op::Add; expected_args = 2; }
        else if (name == "atomic_cas") { op = Op::CAS; expected_args = 3; }
        else if (name == "atomic_load") { op = Op::Load; expected_args = 1; }
        else { op = Op::Store; expected_args = 2; }  // atomic_store
        
        consume(TokenKind::LParen, std::format("expected '(' after '{}'", name));
        
        // first arg is always the pointer
        auto ptr = parse_expr();
        if (!ptr) return std::nullopt;
        
        // remaining args
        std::vector<ast::ExprPtr> args;
        while (match(TokenKind::Comma)) {
            auto arg = parse_expr();
            if (arg) args.push_back(std::move(*arg));
        }
        
        consume(TokenKind::RParen, std::format("expected ')' after {} arguments", name));
        
        // validate arg count: ptr counts as 1, plus the rest
        int total_args = 1 + static_cast<int>(args.size());
        if (total_args != expected_args) {
            error(std::format("{} expects {} arguments, got {}", name, expected_args, total_args));
        }
        
        auto expr = std::make_unique<ast::Expr>();
        expr->kind = ast::AtomicOpExpr{
            .op = op,
            .ptr = std::move(*ptr),
            .args = std::move(args)
        };
        return expr;
    }

    std::optional<ast::ExprPtr> parse_primary() {
        // Literals
        if (match(TokenKind::IntLit)) {
            auto expr = std::make_unique<ast::Expr>();
            std::int64_t val = std::get<std::int64_t>(previous().value);
            expr->kind = ast::LiteralExpr{
                .value = val,
                .type = Type::make_primitive(PrimitiveType::I64)
            };
            return expr;
        }
        if (match(TokenKind::FloatLit)) {
            auto expr = std::make_unique<ast::Expr>();
            double val = std::get<double>(previous().value);
            expr->kind = ast::LiteralExpr{
                .value = val,
                .type = Type::make_primitive(PrimitiveType::F64)
            };
            return expr;
        }
        if (match(TokenKind::StringLit)) {
            auto expr = std::make_unique<ast::Expr>();
            std::string val = std::get<std::string>(previous().value);
            expr->kind = ast::LiteralExpr{
                .value = std::move(val),
                .type = Type::make_primitive(PrimitiveType::Str)
            };
            return expr;
        }
        if (match(TokenKind::HexStringLit)) {
            auto expr = std::make_unique<ast::Expr>();
            std::string val = std::get<std::string>(previous().value);
            expr->kind = ast::LiteralExpr{
                .value = std::move(val),
                .type = Type::make_primitive(PrimitiveType::Str)  // Store as string for now
            };
            return expr;
        }
        if (match(TokenKind::True)) {
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::LiteralExpr{
                .value = true,
                .type = Type::make_primitive(PrimitiveType::Bool)
            };
            return expr;
        }
        if (match(TokenKind::False)) {
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::LiteralExpr{
                .value = false,
                .type = Type::make_primitive(PrimitiveType::Bool)
            };
            return expr;
        }


        // spawn function_name(args) - fires off a thread
        if (match(TokenKind::Spawn)) {
            return parse_spawn_expr();
        }

        // await handle - blocks until thread finishes
        if (match(TokenKind::Await)) {
            return parse_await_expr();
        }

        // Identifiers (and namespace:function calls)
        if (match(TokenKind::Ident)) {
            std::string name(previous().text);
            
            // atomic builtins - intercept before normal call handling
            if (name == "atomic_add" || name == "atomic_cas" || 
                name == "atomic_load" || name == "atomic_store") {
                return parse_atomic_op(name);
            }
            
            // Check for namespace.function() calls (e.g., Mem.Read())
            // Only treat as namespace if:
            // 1. Pattern is: ident.ident(
            // 2. First identifier starts with uppercase (Mem, Array, Console)
            // If lowercase (player, p), let parse_postfix handle as method call
            if (check(TokenKind::Dot) && !name.empty() && name[0] >= 'A' && name[0] <= 'Z') {
                std::size_t saved_pos = pos_;
                advance();  // consume .
                
                if (check(TokenKind::Ident)) {
                    std::size_t after_ident = pos_;
                    Token func_name = advance();
                    
                    // Only treat as namespace if followed by (
                    if (check(TokenKind::LParen)) {
                        // This is namespace.function() call
                        name += ".";
                        name += func_name.text;
                    } else {
                        // Not a function call - restore for field access
                        pos_ = saved_pos;
                    }
                } else {
                    // Not namespace syntax, restore position
                    pos_ = saved_pos;
                }
            }
            
            // Check for struct literal: StructName { field: value, ... }
            // Only if identifier starts with uppercase (class/struct name)
            if (check(TokenKind::LBrace) && !name.empty() && name[0] >= 'A' && name[0] <= 'Z') {
                return parse_struct_literal(name);
            }
            
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::IdentExpr{.name = std::move(name)};
            return expr;
        }
        
        // Type keywords used as variable names (contextual keywords)
        if (current().is_type()) {
            Token tok = advance();
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::IdentExpr{.name = std::string(tok.text)};
            return expr;
        }
        
        // Memory keywords that act like function names
        if (match(TokenKind::Malloc)) {
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::IdentExpr{.name = "malloc"};
            return expr;
        }
        if (match(TokenKind::Free)) {
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::IdentExpr{.name = "free"};  
            return expr;
        }


        // Parenthesized expression
        if (match(TokenKind::LParen)) {
            auto inner = parse_expr();
            consume(TokenKind::RParen, "expected ')'");
            return inner;
        }

        // Array literal
        if (match(TokenKind::LBracket)) {
            std::vector<ast::ExprPtr> elements;
            if (!check(TokenKind::RBracket)) {
                do {
                    auto elem = parse_expr();
                    if (elem) elements.push_back(std::move(*elem));
                } while (match(TokenKind::Comma));
            }
            consume(TokenKind::RBracket, "expected ']'");

            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::ArrayExpr{.elements = std::move(elements)};
            return expr;
        }

        error("expected expression");
        return std::nullopt;
    }
};

} // namespace opus

