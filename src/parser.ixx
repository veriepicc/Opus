// parser - handles c-style and english syntaxes

export module opus.parser;

import opus.types;
import opus.lexer;
import opus.ast;
import opus.errors;
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
    {
        if (tokens_.empty()) {
            Token eof{};
            eof.kind = TokenKind::Eof;
            eof.text = "";
            eof.value = std::monostate{};
            tokens_.push_back(std::move(eof));
        }
    }

    std::expected<ast::Module, std::vector<ParseError>> parse_module(std::string_view name) {
        ast::Module mod;
        mod.name = std::string(name);
        mod.syntax = mode_;
        if (!tokens_.empty()) {
            mod.source_file = tokens_.front().loc.file;
        }

        while (!at_end() && !check(TokenKind::Eof)) {
            if (!check_loop_safeguard("parse_module")) {
                break;
            }
            auto decl = parse_decl();
            if (!decl) {
                synchronize();
            } else {
                mod.decls.push_back(std::move(*decl));
            }
        }

        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }

        for (const auto& issue : ast::validate_module(mod)) {
            errors_.push_back(ParseError{
                .message = issue.message,
                .loc = issue.loc
            });
        }

        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }
        return mod;
    }

    std::expected<ast::ProjectDecl, std::vector<ParseError>> parse_project_file() {
        ast::ProjectDecl project;
        SourceLoc project_loc = tokens_.empty() ? SourceLoc{} : tokens_.front().loc;
        bool found_project = false;

        while (!at_end() && !check(TokenKind::Eof)) {
            if (!check_loop_safeguard("parse_project_file")) {
                break;
            }

            if (match(TokenKind::Project)) {
                SourceLoc decl_loc = previous().loc;
                auto parsed = parse_project_decl();
                if (!parsed) {
                    synchronize();
                    continue;
                }

                if (found_project) {
                    error_at(decl_loc, "multiple project declarations found in opus.project");
                    continue;
                }

                project = std::move(*parsed);
                project_loc = decl_loc;
                found_project = true;
                continue;
            }

            error("unexpected top-level declaration in opus.project");
            synchronize();
        }

        if (!found_project && errors_.empty()) {
            error_at(project_loc, "no project declaration found in opus.project");
        }

        if (found_project) {
            for (const auto& issue : ast::validate_project_decl(project, project_loc)) {
                errors_.push_back(ParseError{
                    .message = issue.message,
                    .loc = issue.loc
                });
            }
        }

        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }

        return project;
    }

private:
    std::vector<Token> tokens_;
    SyntaxMode mode_;
    std::size_t pos_;
    std::vector<ParseError> errors_;
    std::size_t loop_counter_;
    std::size_t max_iterations_;
    bool safeguard_triggered_ = false;

    enum class NameMode {
        Standard,
        AllowTypeKeywords,
    };

    enum class StatementMode {
        CStyle,
        V2,
        English,
        Mixed,
    };

    enum class BracedBodyMode {
        V2,
        Mixed,
    };

    enum class RecoveryMode {
        Declaration,
        Statement,
        ClassMember,
        StructMember,
        EnumMember,
        ProjectProperty,
        StructLiteralField,
    };

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
    
    // ========================================================================
    // TOKEN MANIPULATION
    // ========================================================================

    bool at_end() const { return pos_ >= tokens_.size(); }
    
    const Token& peek(std::size_t offset = 0) const {
        if (tokens_.empty()) {
            std::abort();
        }
        if (pos_ + offset >= tokens_.size()) {
            return tokens_.back(); // EOF token
        }
        return tokens_[pos_ + offset];
    }

    const Token& current() const { return peek(0); }
    const Token& previous() const {
        if (tokens_.empty()) {
            std::abort();
        }
        if (pos_ == 0) {
            return tokens_.front();
        }
        return tokens_[pos_ - 1];
    }

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

    Token consume_keyword_spelling(TokenKind kind, std::string_view spelling, std::string_view message) {
        if (check(kind)) {
            return advance();
        }
        if (token_has_close_keyword_spelling(current(), spelling)) {
            std::vector<std::string> candidates{std::string(spelling)};
            error(with_suggestion(std::string(message), current().text, candidates));
            return current();
        }
        error(message);
        return current();
    }

    bool consume_optional_keyword_spelling(TokenKind kind, std::string_view spelling, std::string_view message) {
        if (check(kind)) {
            advance();
            return true;
        }
        if (token_has_close_keyword_spelling(current(), spelling)) {
            std::vector<std::string> candidates{std::string(spelling)};
            error(with_suggestion(std::string(message), current().text, candidates));
            advance();
            return true;
        }
        return false;
    }

    [[nodiscard]] std::string describe_token(const Token& tok) const {
        if (tok.kind == TokenKind::Eof) {
            return "EOF";
        }
        if (!tok.text.empty()) {
            return std::format("'{}'", tok.text);
        }
        return "<token>";
    }

    [[nodiscard]] bool is_hard_reserved_name_keyword(TokenKind kind) const {
        switch (kind) {
            case TokenKind::Function:
            case TokenKind::Fn:
            case TokenKind::Class:
            case TokenKind::Struct:
            case TokenKind::Enum:
            case TokenKind::Project:
            case TokenKind::Import:
            case TokenKind::Using:
            case TokenKind::Extern:
            case TokenKind::Let:
            case TokenKind::Var:
            case TokenKind::Const:
            case TokenKind::Auto:
            case TokenKind::If:
            case TokenKind::Else:
            case TokenKind::While:
            case TokenKind::For:
            case TokenKind::Loop:
            case TokenKind::Parallel:
            case TokenKind::Break:
            case TokenKind::Continue:
            case TokenKind::Return:
            case TokenKind::Thread:
            case TokenKind::Unsafe:
            case TokenKind::Spawn:
            case TokenKind::Await:
            case TokenKind::Define:
            case TokenKind::Variable:
            case TokenKind::Create:
            case TokenKind::Set:
            case TokenKind::Call:
            case TokenKind::End:
            case TokenKind::Is:
            case TokenKind::Than:
            case TokenKind::Greater:
            case TokenKind::Less:
            case TokenKind::Equal:
            case TokenKind::Begin:
            case TokenKind::Then:
            case TokenKind::Do:
            case TokenKind::With:
            case TokenKind::As:
            case TokenKind::In:
            case TokenKind::Returning:
            case TokenKind::To:
            case TokenKind::Until:
            case TokenKind::Repeat:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] bool is_contextual_name_token(
        const Token& tok,
        NameMode mode = NameMode::AllowTypeKeywords) const {
        if (tok.kind == TokenKind::Ident || tok.kind == TokenKind::Value) {
            return true;
        }
        if (mode == NameMode::AllowTypeKeywords && tok.is_type()) {
            return true;
        }
        if (tok.kind == TokenKind::True || tok.kind == TokenKind::False ||
            tok.kind == TokenKind::Eof || tok.kind == TokenKind::Error) {
            return false;
        }
        if (tok.is_literal() || tok.is_operator()) {
            return false;
        }
        switch (tok.kind) {
            case TokenKind::LParen:
            case TokenKind::RParen:
            case TokenKind::LBrace:
            case TokenKind::RBrace:
            case TokenKind::LBracket:
            case TokenKind::RBracket:
            case TokenKind::Comma:
            case TokenKind::Colon:
            case TokenKind::Semicolon:
            case TokenKind::Dot:
            case TokenKind::Arrow:
            case TokenKind::FatArrow:
            case TokenKind::ColonColon:
            case TokenKind::At:
            case TokenKind::Hash:
            case TokenKind::Question:
            case TokenKind::Newline:
                return false;
            default:
                break;
        }
        if (is_hard_reserved_name_keyword(tok.kind)) {
            return false;
        }
        return tok.is_keyword();
    }

    [[nodiscard]] bool is_contextual_name_token(const Token& tok, bool allow_type_keywords) const {
        return is_contextual_name_token(
            tok,
            allow_type_keywords ? NameMode::AllowTypeKeywords : NameMode::Standard);
    }

    [[nodiscard]] bool can_begin_ambiguous_type_name(const Token& tok) const {
        return tok.kind == TokenKind::Ident || tok.kind == TokenKind::Value || tok.is_type();
    }

    [[nodiscard]] static bool keyword_spelling_is_close(
        std::string_view actual,
        std::string_view expected) {
        if (actual == expected) {
            return true;
        }

        auto actual_len = actual.size();
        auto expected_len = expected.size();
        if (actual_len + 1 < expected_len || expected_len + 1 < actual_len) {
            return false;
        }

        if (actual_len == expected_len) {
            std::vector<std::size_t> mismatches;
            for (std::size_t i = 0; i < actual_len; ++i) {
                if (actual[i] != expected[i]) {
                    mismatches.push_back(i);
                    if (mismatches.size() > 2) {
                        return false;
                    }
                }
            }

            if (mismatches.size() == 1) {
                return true;
            }
            if (mismatches.size() == 2) {
                auto first = mismatches[0];
                auto second = mismatches[1];
                return second == first + 1 &&
                       actual[first] == expected[second] &&
                       actual[second] == expected[first];
            }
            return false;
        }

        auto one_insert_away = [](std::string_view shorter, std::string_view longer) {
            std::size_t i = 0;
            std::size_t j = 0;
            bool skipped = false;
            while (i < shorter.size() && j < longer.size()) {
                if (shorter[i] == longer[j]) {
                    ++i;
                    ++j;
                    continue;
                }
                if (skipped) {
                    return false;
                }
                skipped = true;
                ++j;
            }
            return true;
        };

        if (actual_len + 1 == expected_len) {
            return one_insert_away(actual, expected);
        }
        if (expected_len + 1 == actual_len) {
            return one_insert_away(expected, actual);
        }
        return false;
    }

    [[nodiscard]] bool token_has_close_keyword_spelling(
        const Token& tok,
        std::string_view spelling) const {
        if (!is_contextual_name_token(tok, NameMode::Standard)) {
            return false;
        }
        return keyword_spelling_is_close(tok.text, spelling);
    }

    [[nodiscard]] static const std::vector<std::string>& c_decl_keyword_candidates() {
        static const std::vector<std::string> values{
            "function", "fn", "class", "struct", "enum",
            "const", "import", "using", "extern", "let", "var", "project"
        };
        return values;
    }

    [[nodiscard]] static const std::vector<std::string>& mixed_decl_keyword_candidates() {
        static const std::vector<std::string> values{
            "function", "fn", "class", "struct", "enum",
            "const", "import", "using", "extern", "let", "var", "project",
            "define", "create"
        };
        return values;
    }

    [[nodiscard]] static const std::vector<std::string>& english_decl_keyword_candidates() {
        static const std::vector<std::string> values{
            "define", "create", "function", "struct"
        };
        return values;
    }

    [[nodiscard]] static const std::vector<std::string>& class_member_keyword_candidates() {
        static const std::vector<std::string> values{"function", "fn"};
        return values;
    }

    [[nodiscard]] static const std::vector<std::string>& statement_keyword_candidates() {
        static const std::vector<std::string> values{
            "let", "var", "auto", "const", "return", "if", "while",
            "parallel", "for", "loop", "break", "continue",
            "create", "set", "call"
        };
        return values;
    }

    [[nodiscard]] static const std::vector<std::string>& project_property_candidates() {
        static const std::vector<std::string> values{
            "entry", "output", "mode", "include", "debug", "healing"
        };
        return values;
    }

    [[nodiscard]] static const std::vector<std::string>& bool_value_candidates() {
        static const std::vector<std::string> values{"true", "false"};
        return values;
    }

    [[nodiscard]] static const std::vector<std::string>& healing_mode_candidates() {
        static const std::vector<std::string> values{"auto", "freeze", "off"};
        return values;
    }

    [[nodiscard]] static const std::vector<std::string>& project_mode_candidates() {
        static const std::vector<std::string> values{"dll", "exe"};
        return values;
    }

    [[nodiscard]] std::string with_suggestion(
        std::string message,
        std::string_view input,
        const std::vector<std::string>& candidates) const {
        if (auto suggestion = find_closest_match(input, candidates)) {
            message += std::format(" (did you mean '{}'?)", *suggestion);
        }
        return message;
    }

    void error(std::string_view message) {
        std::string full(message);
        full += std::format(", found {}", describe_token(current()));
        errors_.push_back(ParseError{
            .message = std::move(full),
            .loc = current().loc
        });
    }

    void error_at(const SourceLoc& loc, std::string_view message) {
        errors_.push_back(ParseError{
            .message = std::string(message),
            .loc = loc
        });
    }

    void error_invalid_project_property(std::string_view key) {
        error(with_suggestion(
            std::format("unknown project property: '{}'", key),
            key,
            project_property_candidates()));
    }

    void error_expected_decl_start(const std::vector<std::string>& candidates, std::string_view fallback) {
        if (is_contextual_name_token(current(), NameMode::Standard)) {
            error(with_suggestion(std::string(fallback), current().text, candidates));
            return;
        }
        error(fallback);
    }

    [[nodiscard]] bool maybe_report_keyword_typo(std::string_view spelling, std::string_view message) {
        if (!token_has_close_keyword_spelling(current(), spelling)) {
            return false;
        }
        std::vector<std::string> candidates{std::string(spelling)};
        error(with_suggestion(std::string(message), current().text, candidates));
        return true;
    }

    [[nodiscard]] bool looks_like_identifier_expr_statement_tail() const {
        switch (peek(1).kind) {
            case TokenKind::LParen:
            case TokenKind::LBracket:
            case TokenKind::Dot:
            case TokenKind::As:
            case TokenKind::PlusPlus:
            case TokenKind::MinusMinus:
            case TokenKind::Assign:
            case TokenKind::PlusEq:
            case TokenKind::MinusEq:
            case TokenKind::StarEq:
            case TokenKind::SlashEq:
            case TokenKind::PercentEq:
            case TokenKind::Plus:
            case TokenKind::Minus:
            case TokenKind::Star:
            case TokenKind::Slash:
            case TokenKind::Percent:
            case TokenKind::Eq:
            case TokenKind::Ne:
            case TokenKind::Lt:
            case TokenKind::Gt:
            case TokenKind::Le:
            case TokenKind::Ge:
            case TokenKind::AndAnd:
            case TokenKind::OrOr:
            case TokenKind::Ampersand:
            case TokenKind::Pipe:
            case TokenKind::Caret:
            case TokenKind::Shl:
            case TokenKind::Shr:
            case TokenKind::Semicolon:
            case TokenKind::RBrace:
            case TokenKind::End:
            case TokenKind::Else:
            case TokenKind::Eof:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] bool maybe_report_statement_keyword_typo() {
        if (!is_contextual_name_token(current(), NameMode::Standard)) {
            return false;
        }
        if (looks_like_identifier_expr_statement_tail()) {
            return false;
        }
        auto suggestion = find_closest_match(current().text, statement_keyword_candidates(), 2);
        if (!suggestion) {
            return false;
        }
        error(std::format(
            "unrecognized statement start '{}' (did you mean '{}'?)",
            current().text,
            *suggestion));
        return true;
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

            if (looks_like_decl_start()) {
                return;
            }
            advance();
        }
    }

    [[nodiscard]] bool looks_like_type_first_fn_decl_start() const {
        if (current().is_type()) return true;
        return can_begin_ambiguous_type_name(current()) &&
               is_contextual_name_token(peek(1), true) &&
               peek(2).kind == TokenKind::LParen;
    }

    [[nodiscard]] bool looks_like_type_first_signature() const {
        return can_begin_ambiguous_type_name(current()) &&
               is_contextual_name_token(peek(1), true) &&
               peek(2).kind == TokenKind::LParen;
    }

    [[nodiscard]] bool keyword_appears_before_block(TokenKind keyword) const {
        std::int32_t paren_depth = 0;
        std::int32_t bracket_depth = 0;
        for (std::size_t i = 1; i < 32 && pos_ + i < tokens_.size(); ++i) {
            const auto kind = peek(i).kind;
            if (kind == TokenKind::LParen) {
                ++paren_depth;
                continue;
            }
            if (kind == TokenKind::RParen) {
                if (paren_depth > 0) --paren_depth;
                continue;
            }
            if (kind == TokenKind::LBracket) {
                ++bracket_depth;
                continue;
            }
            if (kind == TokenKind::RBracket) {
                if (bracket_depth > 0) --bracket_depth;
                continue;
            }

            if (paren_depth == 0 && bracket_depth == 0) {
                if (kind == keyword) {
                    return true;
                }
                if (kind == TokenKind::LBrace ||
                    kind == TokenKind::Semicolon ||
                    kind == TokenKind::RBrace ||
                    kind == TokenKind::End ||
                    kind == TokenKind::Eof ||
                    kind == TokenKind::Newline) {
                    return false;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool keyword_or_close_spelling_appears_before_block(
        TokenKind keyword,
        std::string_view spelling) const {
        std::int32_t paren_depth = 0;
        std::int32_t bracket_depth = 0;
        for (std::size_t i = 1; i < 32 && pos_ + i < tokens_.size(); ++i) {
            const auto& tok = peek(i);
            const auto kind = tok.kind;
            if (kind == TokenKind::LParen) {
                ++paren_depth;
                continue;
            }
            if (kind == TokenKind::RParen) {
                if (paren_depth > 0) --paren_depth;
                continue;
            }
            if (kind == TokenKind::LBracket) {
                ++bracket_depth;
                continue;
            }
            if (kind == TokenKind::RBracket) {
                if (bracket_depth > 0) --bracket_depth;
                continue;
            }

            if (paren_depth == 0 && bracket_depth == 0) {
                if (kind == keyword || token_has_close_keyword_spelling(tok, spelling)) {
                    return true;
                }
                if (kind == TokenKind::LBrace ||
                    kind == TokenKind::Semicolon ||
                    kind == TokenKind::RBrace ||
                    kind == TokenKind::End ||
                    kind == TokenKind::Eof ||
                    kind == TokenKind::Newline) {
                    return false;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool looks_like_english_if_stmt() const {
        return check(TokenKind::If) &&
               keyword_or_close_spelling_appears_before_block(TokenKind::Then, "then");
    }

    [[nodiscard]] bool looks_like_english_while_stmt() const {
        return check(TokenKind::While) &&
               keyword_or_close_spelling_appears_before_block(TokenKind::Do, "do");
    }

    [[nodiscard]] bool looks_like_struct_literal_after(std::size_t brace_offset = 0) const {
        if (peek(brace_offset).kind != TokenKind::LBrace) {
            return false;
        }
        if (peek(brace_offset + 1).kind == TokenKind::RBrace) {
            return true;
        }
        if (!is_contextual_name_token(peek(brace_offset + 1), true)) {
            return false;
        }

        const auto next_kind = peek(brace_offset + 2).kind;
        return next_kind == TokenKind::Colon ||
               next_kind == TokenKind::Comma ||
               next_kind == TokenKind::RBrace;
    }

    Token consume_name(
        std::string_view message,
        NameMode mode = NameMode::AllowTypeKeywords) {
        if (is_contextual_name_token(current(), mode)) {
            return advance();
        }
        error(message);
        return current();
    }

    Token consume_name(std::string_view message, bool allow_type_keywords) {
        return consume_name(
            message,
            allow_type_keywords ? NameMode::AllowTypeKeywords : NameMode::Standard);
    }

    [[nodiscard]] StatementMode mode_for_braced_body(BracedBodyMode mode) const {
        switch (mode) {
            case BracedBodyMode::V2:
                return StatementMode::V2;
            case BracedBodyMode::Mixed:
                return StatementMode::Mixed;
        }
        return StatementMode::V2;
    }

    [[nodiscard]] bool can_resume_after_failure(RecoveryMode mode) const {
        switch (mode) {
            case RecoveryMode::Declaration:
                return looks_like_decl_start();
            case RecoveryMode::Statement:
                return looks_like_stmt_start();
            case RecoveryMode::ClassMember:
                return looks_like_class_member_start();
            case RecoveryMode::StructMember:
                return looks_like_struct_member_start();
            case RecoveryMode::EnumMember:
                return looks_like_enum_member_start();
            case RecoveryMode::ProjectProperty:
                return looks_like_project_property_start();
            case RecoveryMode::StructLiteralField:
                return is_contextual_name_token(current(), NameMode::AllowTypeKeywords);
        }
        return false;
    }

    [[nodiscard]] bool looks_like_typed_name_start() const {
        if (current().is_type()) return true;
        return can_begin_ambiguous_type_name(current()) &&
               is_contextual_name_token(peek(1), true);
    }

    [[nodiscard]] bool looks_like_decl_start() const {
        return check(TokenKind::Function) ||
               check(TokenKind::Class) ||
               check(TokenKind::Fn) ||
               check(TokenKind::Struct) ||
               check(TokenKind::Enum) ||
               check(TokenKind::Const) ||
               check(TokenKind::Import) ||
               check(TokenKind::Using) ||
               check(TokenKind::Extern) ||
               check(TokenKind::Project) ||
               check(TokenKind::Let) ||
               check(TokenKind::Var) ||
               check(TokenKind::Define) ||
               check(TokenKind::Create) ||
               looks_like_type_first_fn_decl_start();
    }

    [[nodiscard]] bool looks_like_stmt_start() const {
        return check(TokenKind::Let) ||
               check(TokenKind::Var) ||
               check(TokenKind::Auto) ||
               check(TokenKind::Const) ||
               check(TokenKind::Return) ||
               check(TokenKind::If) ||
               check(TokenKind::While) ||
               check(TokenKind::Parallel) ||
               check(TokenKind::For) ||
               check(TokenKind::Loop) ||
               check(TokenKind::Break) ||
               check(TokenKind::Continue) ||
               check(TokenKind::LBrace) ||
               check(TokenKind::Create) ||
               check(TokenKind::Set) ||
               check(TokenKind::Call) ||
               looks_like_english_if_stmt() ||
               looks_like_english_while_stmt() ||
               looks_like_typed_name_start();
    }

    [[nodiscard]] bool looks_like_expr_start() const {
        return check(TokenKind::IntLit) ||
               check(TokenKind::FloatLit) ||
               check(TokenKind::StringLit) ||
               check(TokenKind::HexStringLit) ||
               check(TokenKind::True) ||
               check(TokenKind::False) ||
               check(TokenKind::Spawn) ||
               check(TokenKind::Thread) ||
               check(TokenKind::Await) ||
               check(TokenKind::LParen) ||
               check(TokenKind::LBracket) ||
               check(TokenKind::Minus) ||
               check(TokenKind::Bang) ||
               check(TokenKind::Not) ||
               check(TokenKind::Tilde) ||
               check(TokenKind::Star) ||
               check(TokenKind::Ampersand) ||
               check(TokenKind::PlusPlus) ||
               check(TokenKind::MinusMinus) ||
               is_contextual_name_token(current(), NameMode::AllowTypeKeywords);
    }

    [[nodiscard]] bool looks_like_class_member_start() const {
        return check(TokenKind::Function) ||
               check(TokenKind::Fn) ||
               looks_like_type_first_signature() ||
               looks_like_typed_name_start() ||
               is_contextual_name_token(current(), true);
    }

    [[nodiscard]] bool looks_like_struct_member_start() const {
        return looks_like_typed_name_start() ||
               is_contextual_name_token(current(), true);
    }

    [[nodiscard]] bool looks_like_enum_member_start() const {
        return is_contextual_name_token(current(), true);
    }

    [[nodiscard]] bool looks_like_project_property_start() const {
        return check(TokenKind::Include) || is_contextual_name_token(current(), true);
    }

    template<typename StopFn>
    void recover_after_failure(StopFn&& should_stop, RecoveryMode recovery_mode) {
        if (!at_end()) {
            advance();
        }

        std::size_t safety = 0;
        while (!should_stop() && !at_end()) {
            if (++safety > 10000) {
                std::cerr << "[OPUS ERROR] local recovery exceeded 10000 iterations, aborting\n";
                return;
            }
            if (previous().kind == TokenKind::Semicolon || previous().kind == TokenKind::Comma) {
                return;
            }
            if (can_resume_after_failure(recovery_mode)) {
                return;
            }
            advance();
        }
    }

    template<typename StopFn>
    ast::StmtSuite parse_stmt_suite_until(StatementMode mode, StopFn&& should_stop, const char* label) {
        ast::StmtBlock stmts;
        while (!should_stop() && !at_end()) {
            if (!check_loop_safeguard(label)) break;

            std::size_t before = pos_;
            auto stmt = parse_stmt_in(mode);
            if (stmt) {
                stmts.push_back(std::move(*stmt));
            } else if (pos_ == before && !at_end()) {
                recover_after_failure([&]() { return should_stop(); }, RecoveryMode::Statement);
            }
        }
        return ast::StmtSuite{std::move(stmts)};
    }

    ast::StmtSuite parse_stmt_suite_until_end(StatementMode mode, const char* label) {
        return parse_stmt_suite_until(
            mode,
            [this]() { return check(TokenKind::End); },
            label);
    }

    ast::StmtSuite parse_stmt_suite_until_else_or_end(StatementMode mode, const char* label) {
        return parse_stmt_suite_until(
            mode,
            [this]() { return check(TokenKind::Else) || check(TokenKind::End); },
            label);
    }

    template<typename StopFn, typename ParseOneFn>
    void parse_recovering_sequence(
        const char* label,
        StopFn&& should_stop,
        RecoveryMode recovery_mode,
        ParseOneFn&& parse_one) {
        while (!should_stop() && !at_end()) {
            if (!check_loop_safeguard(label)) {
                break;
            }

            std::size_t before = pos_;
            bool progressed = parse_one();
            if (!progressed && pos_ == before && !at_end()) {
                recover_after_failure([&]() { return should_stop(); }, recovery_mode);
            }
        }
    }

    template<typename ParseElementFn, typename StopFn, typename OnElementFn>
    bool parse_comma_separated(ParseElementFn&& parse_element, StopFn&& should_stop, OnElementFn&& on_element) {
        while (!should_stop() && !at_end()) {
            auto element = parse_element();
            if (!element) {
                return false;
            }
            on_element(std::move(*element));
            if (!match(TokenKind::Comma)) {
                break;
            }
            if (should_stop()) {
                break;
            }
        }
        return true;
    }

    template<typename ParseElementFn, typename StopFn, typename OnElementFn, typename CanBeginNextFn>
    bool parse_comma_separated_recovering(
        ParseElementFn&& parse_element,
        StopFn&& should_stop,
        OnElementFn&& on_element,
        std::string_view missing_comma_message,
        CanBeginNextFn&& can_begin_next_without_comma) {
        while (!should_stop() && !at_end()) {
            auto element = parse_element();
            if (!element) {
                return false;
            }
            on_element(std::move(*element));
            if (match(TokenKind::Comma)) {
                if (should_stop()) {
                    break;
                }
                continue;
            }
            if (should_stop()) {
                break;
            }
            if (can_begin_next_without_comma()) {
                error(missing_comma_message);
                continue;
            }
            break;
        }
        return true;
    }

    [[nodiscard]] ast::DeclPtr make_fn_decl(
        std::string name,
        ast::ParamList params,
        Type return_type,
        std::optional<ast::StmtSuite> body,
        SourceSpan span = {},
        ast::DeclAttrs attrs = {}) {
        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::FnDecl{
            .name = std::move(name),
            .params = std::move(params),
            .return_type = std::move(return_type),
            .body = std::move(body),
            .attrs = std::move(attrs),
        };
        decl->span = std::move(span);
        return decl;
    }

    [[nodiscard]] ast::MethodDecl make_instance_method(
        std::string name,
        ast::ParamList params,
        Type return_type,
        ast::StmtSuite body) {
        return ast::MethodDecl{
            .name = std::move(name),
            .params = std::move(params),
            .return_type = std::move(return_type),
            .body = std::move(body),
            .receiver = ast::MethodReceiver::ImplicitSelf
        };
    }

    [[nodiscard]] ast::StmtSuite make_expression_body_suite(Type return_type, ast::ExprPtr expr) {
        auto stmt = std::make_unique<ast::Stmt>();
        if (return_type.is_void()) {
            stmt->kind = ast::ExprStmt{
                .expr = std::move(expr)
            };
        } else {
            stmt->kind = ast::ReturnStmt{
                .value = std::move(expr)
            };
        }

        ast::StmtBlock stmts;
        stmts.push_back(std::move(stmt));
        return ast::StmtSuite{std::move(stmts)};
    }

    struct ParsedCallableSignature {
        std::string name;
        ast::ParamList params;
        Type return_type = Type::make_primitive(PrimitiveType::Void);
    };

    std::optional<ParsedCallableSignature> parse_signature_type_name_params(
        std::string_view name_message,
        NameMode name_mode = NameMode::AllowTypeKeywords) {
        auto ret_type = parse_type();
        if (!ret_type) {
            return std::nullopt;
        }

        Token name_tok = consume_name(name_message, name_mode);
        consume(TokenKind::LParen, "expected '(' after function name");
        auto params = parse_param_list_flexible();
        consume(TokenKind::RParen, "expected ')' after parameters");

        return ParsedCallableSignature{
            .name = std::string(name_tok.text),
            .params = std::move(params),
            .return_type = std::move(*ret_type)
        };
    }

    std::optional<ParsedCallableSignature> parse_signature_name_params_optional_return(
        std::string_view name_message,
        NameMode name_mode = NameMode::AllowTypeKeywords) {
        Token name_tok = consume_name(name_message, name_mode);
        consume(TokenKind::LParen, "expected '(' after function name");
        auto params = parse_param_list_flexible();
        consume(TokenKind::RParen, "expected ')' after parameters");

        Type ret_type = Type::make_primitive(PrimitiveType::Void);
        if (match(TokenKind::Arrow) || match(TokenKind::Colon)) {
            auto parsed_ret = parse_type();
            if (parsed_ret) {
                ret_type = std::move(*parsed_ret);
            }
        }

        return ParsedCallableSignature{
            .name = std::string(name_tok.text),
            .params = std::move(params),
            .return_type = std::move(ret_type)
        };
    }

    std::optional<ParsedCallableSignature> parse_english_function_signature() {
        Token name_tok = consume_name("expected function name", NameMode::AllowTypeKeywords);

        ast::ParamList params;
        if (match(TokenKind::With)) {
            bool ok = parse_comma_separated_recovering(
                [this]() -> std::optional<ast::Param> {
                    Token pname = consume_name("expected parameter name");
                    consume_keyword_spelling(TokenKind::As, "as", "expected 'as' after parameter name");
                    auto ptype = parse_type();
                    if (!ptype) {
                        return std::nullopt;
                    }
                    return ast::Param{
                        .name = std::string(pname.text),
                        .type = std::move(*ptype)
                    };
                },
                [this]() {
                    return check(TokenKind::Returning) ||
                           check(TokenKind::FatArrow) ||
                           check(TokenKind::End) ||
                           at_end();
                },
                [&](ast::Param param) {
                    params.push_back(std::move(param));
                },
                "expected ',' between parameters",
                [this]() { return is_contextual_name_token(current(), NameMode::Standard); });
            if (!ok) {
                return std::nullopt;
            }
        } else if (maybe_report_keyword_typo("with", "expected 'with'")) {
            return std::nullopt;
        }

        Type ret_type = Type::make_primitive(PrimitiveType::Void);
        if (match(TokenKind::Returning)) {
            auto parsed_ret = parse_type();
            if (parsed_ret) {
                ret_type = std::move(*parsed_ret);
            }
        } else if (maybe_report_keyword_typo("returning", "expected 'returning'")) {
            return std::nullopt;
        }

        return ParsedCallableSignature{
            .name = std::string(name_tok.text),
            .params = std::move(params),
            .return_type = std::move(ret_type)
        };
    }

    std::optional<ast::StmtSuite> parse_decl_body_suite(
        Type return_type,
        BracedBodyMode body_mode,
        const char* label) {
        if (match(TokenKind::FatArrow)) {
            auto expr = parse_expr();
            if (!expr) {
                error(std::format("expected expression after {}", label));
                return std::nullopt;
            }
            match(TokenKind::Semicolon);
            return make_expression_body_suite(std::move(return_type), std::move(*expr));
        }

        consume(TokenKind::LBrace, std::format("expected '{{' or '=>' after {}", label));
        auto body = parse_block_stmts(mode_for_braced_body(body_mode), "parse_decl_body_suite");
        consume(TokenKind::RBrace, "expected '}' after function body");
        return body;
    }

    std::optional<ast::MethodDecl> parse_class_method_function_style() {
        auto sig = parse_signature_type_name_params("expected method name");
        if (!sig) {
            return std::nullopt;
        }

        auto body = parse_decl_body_suite(sig->return_type.clone(), BracedBodyMode::V2, "method signature");
        if (!body) {
            return std::nullopt;
        }

        return make_instance_method(
            std::move(sig->name),
            std::move(sig->params),
            std::move(sig->return_type),
            std::move(*body));
    }

    std::optional<ast::MethodDecl> parse_class_method_fn_style() {
        auto sig = parse_signature_name_params_optional_return("expected method name", NameMode::AllowTypeKeywords);
        if (!sig) {
            return std::nullopt;
        }

        auto body = parse_decl_body_suite(sig->return_type.clone(), BracedBodyMode::V2, "method signature");
        if (!body) {
            return std::nullopt;
        }

        return make_instance_method(
            std::move(sig->name),
            std::move(sig->params),
            std::move(sig->return_type),
            std::move(*body));
    }

    std::optional<ast::MethodDecl> parse_class_method_type_first() {
        auto sig = parse_signature_type_name_params("expected method name after type");
        if (!sig) {
            return std::nullopt;
        }

        auto body = parse_decl_body_suite(sig->return_type.clone(), BracedBodyMode::V2, "method signature");
        if (!body) {
            return std::nullopt;
        }

        return make_instance_method(
            std::move(sig->name),
            std::move(sig->params),
            std::move(sig->return_type),
            std::move(*body));
    }

    // ========================================================================
    // DECLARATIONS
    // ========================================================================

    std::optional<ast::DeclPtr> parse_decl() {
        if (check(TokenKind::Define) || check(TokenKind::Create)) {
            return parse_decl_english();
        }
        return parse_decl_c();
    }

    std::optional<ast::DeclPtr> parse_decl_c() {
        if (match(TokenKind::Function)) {
            return parse_fn_decl_v2();
        }
        if (match(TokenKind::Class)) {
            return parse_class_decl();
        }
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
        if (match(TokenKind::Using)) {
            return parse_type_alias_decl();
        }
        if (match(TokenKind::Extern)) {
            return parse_extern_decl();
        }
        if (match(TokenKind::Project)) {
            SourceLoc project_loc = previous().loc;
            auto ignored = parse_project_decl();
            if (!ignored) {
                synchronize();
            }
            error_at(project_loc, "project declarations are only valid in opus.project");
            return std::nullopt;
        }
        if (match(TokenKind::Let)) {
            return parse_static_decl(false);
        }
        if (match(TokenKind::Var)) {
            return parse_static_decl(true);
        }
        if (looks_like_type_first_fn_decl_start()) {
            return parse_fn_decl_v2_type_first();
        }
        
        error_expected_decl_start(
            mixed_decl_keyword_candidates(),
            "expected declaration (function, class, struct, etc.)");
        return std::nullopt;
    }

    // English: define function name returning Type ... end function
    std::optional<ast::DeclPtr> parse_decl_english() {
        if (match(TokenKind::Define)) {
            if (match(TokenKind::Function)) {
                return parse_fn_decl_english();
            }
        }
        if (match(TokenKind::Create)) {
            if (match(TokenKind::Struct)) {
                return parse_struct_decl();
            }
        }
        
        error_expected_decl_start(
            english_decl_keyword_candidates(),
            "expected declaration (define function, create struct, etc.)");
        return std::nullopt;
    }

    // ========================================================================
    // FUNCTION DECLARATIONS
    // ========================================================================

    std::optional<ast::DeclPtr> parse_fn_decl_c() {
        SourceSpan span{.start = previous().loc};

        auto sig = parse_signature_name_params_optional_return("expected function name", NameMode::AllowTypeKeywords);
        if (!sig) {
            return std::nullopt;
        }

        auto body = parse_decl_body_suite(sig->return_type.clone(), BracedBodyMode::Mixed, "function signature");
        if (!body) {
            return std::nullopt;
        }

        span.end = previous().loc;

        return make_fn_decl(
            std::move(sig->name),
            std::move(sig->params),
            std::move(sig->return_type),
            std::optional<ast::StmtSuite>{std::move(*body)},
            span);
    }

    std::optional<ast::DeclPtr> parse_fn_decl_english() {
        SourceSpan span{.start = previous().loc};

        auto sig = parse_english_function_signature();
        if (!sig) {
            return std::nullopt;
        }

        if (match(TokenKind::FatArrow)) {
            auto expr = parse_expr();
            if (!expr) {
                error("expected expression after =>");
                return std::nullopt;
            }

            auto body = make_expression_body_suite(sig->return_type.clone(), std::move(*expr));
            match(TokenKind::Semicolon);
            if (match(TokenKind::End)) {
                consume_keyword_spelling(TokenKind::Function, "function", "expected 'function' after 'end'");
            }
            span.end = previous().loc;

            return make_fn_decl(
                std::move(sig->name),
                std::move(sig->params),
                std::move(sig->return_type),
                std::optional<ast::StmtSuite>{std::move(body)},
                span);
        }

        auto body = parse_stmt_suite_until_end(StatementMode::Mixed, "parse_fn_decl_english");
        
        consume_keyword_spelling(TokenKind::End, "end", "expected 'end'");
        consume_keyword_spelling(TokenKind::Function, "function", "expected 'function' after 'end'");

        span.end = previous().loc;

        return make_fn_decl(
            std::move(sig->name),
            std::move(sig->params),
            std::move(sig->return_type),
            std::optional<ast::StmtSuite>{std::move(body)},
            span);
    }

    // ========================================================================
    // v2.0 FUNCTION DECLARATIONS - C++/JS Hybrid Style
    // ========================================================================

    // v2.0: function RetType name(Type param, Type param) { body }
    std::optional<ast::DeclPtr> parse_fn_decl_v2() {
        SourceSpan span{.start = previous().loc};
        
        bool is_thread = match(TokenKind::Thread);
        (void)is_thread; // todo: wire up to FnDecl when threading lands

        auto sig = parse_signature_type_name_params("expected function name", NameMode::AllowTypeKeywords);
        if (!sig) {
            return std::nullopt;
        }

        auto body = parse_decl_body_suite(sig->return_type.clone(), BracedBodyMode::V2, "function signature");
        if (!body) {
            return std::nullopt;
        }

        span.end = previous().loc;

        return make_fn_decl(
            std::move(sig->name),
            std::move(sig->params),
            std::move(sig->return_type),
            std::optional<ast::StmtSuite>{std::move(*body)},
            span);
    }

    // v2.0: Type name(Type param, ...) { body } - no function keyword
    std::optional<ast::DeclPtr> parse_fn_decl_v2_type_first() {
        SourceSpan span{.start = current().loc};

        auto sig = parse_signature_type_name_params("expected function name after type", NameMode::AllowTypeKeywords);
        if (!sig) {
            return std::nullopt;
        }

        auto body = parse_decl_body_suite(sig->return_type.clone(), BracedBodyMode::V2, "function signature");
        if (!body) {
            return std::nullopt;
        }

        span.end = previous().loc;

        return make_fn_decl(
            std::move(sig->name),
            std::move(sig->params),
            std::move(sig->return_type),
            std::optional<ast::StmtSuite>{std::move(*body)},
            span);
    }

    // v2.0 param list: Type name, Type name, ...
    std::vector<ast::Param> parse_param_list_v2() {
        std::vector<ast::Param> params;
        if (check(TokenKind::RParen)) return params;

        bool ok = parse_comma_separated_recovering(
            [this]() -> std::optional<ast::Param> {
                auto ptype = parse_type();
                if (!ptype) {
                    error("expected parameter type");
                    return std::nullopt;
                }
                Token pname = consume_name("expected parameter name after type");
                return ast::Param{
                    .name = std::string(pname.text),
                    .type = std::move(*ptype),
                    .is_mut = true
                };
            },
            [this]() { return check(TokenKind::RParen) || at_end(); },
            [&](ast::Param param) {
                params.push_back(std::move(param));
            },
            "expected ',' between parameters",
            [this]() { return looks_like_typed_name_start(); });
        if (!ok) {
            return params;
        }

        return params;
    }

    [[nodiscard]] bool looks_like_name_first_param() const {
        if (check(TokenKind::Mut)) {
            return is_contextual_name_token(peek(1), true) && peek(2).kind == TokenKind::Colon;
        }
        return is_contextual_name_token(current(), true) && peek(1).kind == TokenKind::Colon;
    }

    std::vector<ast::Param> parse_param_list_flexible() {
        if (looks_like_name_first_param()) {
            return parse_param_list();
        }
        return parse_param_list_v2();
    }

    ast::StmtSuite parse_block_stmts(
        StatementMode mode,
        const char* label,
        bool also_stop_at_end = false) {
        return parse_stmt_suite_until(
            mode,
            [this, also_stop_at_end]() {
                return check(TokenKind::RBrace) || (also_stop_at_end && check(TokenKind::End));
            },
            label);
    }

    std::optional<ast::StmtSuite> parse_stmt_suite_or_single(
        StatementMode mode,
        const char* block_label,
        std::string_view missing_close_message) {
        if (match(TokenKind::LBrace)) {
            auto body = parse_block_stmts(mode, block_label);
            consume(TokenKind::RBrace, missing_close_message);
            return body;
        }

        auto stmt = parse_stmt_in(mode);
        if (!stmt) {
            return std::nullopt;
        }

        ast::StmtBlock stmts;
        stmts.push_back(std::move(*stmt));
        return ast::StmtSuite{std::move(stmts)};
    }

    std::optional<ast::StmtPtr> parse_stmt_v2() {
        SourceLoc stmt_start = current().loc;
        
        if (match(TokenKind::Let)) {
            bool is_mut = match(TokenKind::Mut);
            auto s = parse_let_or_var_v2(is_mut);
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Var)) {
            auto s = parse_let_or_var_v2(true);
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        // auto is just var with type inference
        if (match(TokenKind::Auto)) {
            auto s = parse_let_or_var_v2(true);
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Const)) {
            auto s = parse_const_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (looks_like_typed_name_start()) {
            auto s = parse_type_decl_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Return)) {
            auto s = parse_return_stmt();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::If)) {
            auto s = parse_if_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::While)) {
            auto s = parse_while_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Parallel)) {
            auto s = parse_parallel_for();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::For)) {
            auto s = parse_for_stmt_v2();
            if (s) (*s)->span.start = stmt_start;
            return s;
        }
        if (match(TokenKind::Loop)) {
            auto s = parse_loop_stmt(StatementMode::V2);
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
            auto stmts = parse_block_stmts(StatementMode::V2, "parse_block_stmts_v2");
            consume(TokenKind::RBrace, "expected '}'");
            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::BlockStmt{.block = ast::Block{.stmts = std::move(stmts)}};
            stmt->span.start = stmt_start;
            return stmt;
        }

        if (maybe_report_statement_keyword_typo()) {
            return std::nullopt;
        }

        auto expr = parse_expr();
        if (!expr) {
            // advance past bad token to prevent infinite loop
            if (!at_end()) advance();
            return std::nullopt;
        }
        match(TokenKind::Semicolon);
        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ExprStmt{.expr = std::move(*expr)};
        stmt->span.start = stmt_start;
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_let_or_var_v2(bool is_mut) {
        Token name = consume_name("expected variable name", true);
        
        std::optional<Type> type;
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }

        ast::ExprPtr init;
        if (match(TokenKind::Assign)) {
            if (auto expr = parse_expr()) {
                init = std::move(*expr);
            }
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

    std::optional<ast::StmtPtr> parse_const_stmt_v2() {
        Token name = consume_name("expected constant name");
        
        std::optional<Type> type;
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }

        consume(TokenKind::Assign, "expected '=' after constant name");
        ast::ExprPtr init;
        if (auto expr = parse_expr()) {
            init = std::move(*expr);
        }

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

    std::optional<ast::StmtPtr> parse_type_decl_stmt_v2() {
        auto type = parse_type();
        if (!type) return std::nullopt;
        
        Token name = consume_name("expected variable name after type");
        
        ast::ExprPtr init;
        if (match(TokenKind::Assign)) {
            if (auto expr = parse_expr()) {
                init = std::move(*expr);
            }
        }

        match(TokenKind::Semicolon);

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::LetStmt{
            .name = std::string(name.text),
            .type = std::move(type),
            .init = std::move(init),
            .is_mut = true
        };
        return stmt;
    }

    // no parens on if/while - go-style
    std::optional<ast::StmtPtr> parse_if_stmt_v2() {
        auto cond = parse_expr();
        if (!cond) return std::nullopt;

        auto then_block = parse_stmt_suite_or_single(
            StatementMode::V2,
            "parse_if_stmt_v2_then",
            "expected '}' after if body");
        if (!then_block) return std::nullopt;

        std::optional<ast::StmtSuite> else_block;
        if (match(TokenKind::Else)) {
            else_block = parse_stmt_suite_or_single(
                StatementMode::V2,
                "parse_if_stmt_v2_else",
                "expected '}' after else body");
            if (!else_block) return std::nullopt;
        }

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::IfStmt{
            .condition = std::move(*cond),
            .then_block = std::move(*then_block),
            .else_block = std::move(else_block)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_while_stmt_v2() {
        auto cond = parse_expr();
        if (!cond) return std::nullopt;
        auto body = parse_stmt_suite_or_single(
            StatementMode::V2,
            "parse_while_stmt_v2_body",
            "expected '}' after while body");
        if (!body) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::WhileStmt{
            .condition = std::move(*cond),
            .body = std::move(*body)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_for_stmt_v2() {
        Token var = consume_name("expected variable");
        consume(TokenKind::In, "expected 'in'");
        auto iter = parse_expr();
        if (!iter) return std::nullopt;
        auto body = parse_stmt_suite_or_single(
            StatementMode::V2,
            "parse_for_stmt_v2_body",
            "expected '}' after for body");
        if (!body) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ForStmt{
            .name = std::string(var.text),
            .iterable = std::move(*iter),
            .body = std::move(*body)
        };
        return stmt;
    }

    std::optional<ast::DeclPtr> parse_class_decl() {
        Token name_tok = consume_name("expected class name");
        consume(TokenKind::LBrace, "expected '{'");

        ast::FieldDeclList fields;
        std::vector<ast::MethodDecl> methods;

        parse_recovering_sequence(
            "parse_class_decl",
            [this]() { return check(TokenKind::RBrace); },
            RecoveryMode::ClassMember,
            [&]() {
                if (match(TokenKind::Function)) {
                    auto method = parse_class_method_function_style();
                    if (method) {
                        methods.push_back(std::move(*method));
                    }
                    return true;
                }
                if (match(TokenKind::Fn)) {
                    auto method = parse_class_method_fn_style();
                    if (method) {
                        methods.push_back(std::move(*method));
                    }
                    return true;
                }
                if (looks_like_type_first_signature()) {
                    auto method = parse_class_method_type_first();
                    if (method) {
                        methods.push_back(std::move(*method));
                    }
                    return true;
                }
                if (is_contextual_name_token(current(), NameMode::Standard) &&
                    peek(1).kind != TokenKind::Colon) {
                    if (auto suggestion = find_closest_match(current().text, class_member_keyword_candidates())) {
                        error(std::format(
                            "expected field or method in class (did you mean '{}'?)",
                            *suggestion));
                        return false;
                    }
                }
                if (looks_like_typed_name_start() || is_contextual_name_token(current(), NameMode::AllowTypeKeywords)) {
                    if (auto field = parse_field_decl_flexible()) {
                        fields.push_back(std::move(*field));
                    }
                    consume_member_separator();
                    return true;
                }

                error("expected field or method in class");
                return false;
            });

        consume(TokenKind::RBrace, "expected '}'");

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::ClassDecl{
            .name = std::string(name_tok.text),
            .fields = std::move(fields),
            .methods = std::move(methods)
        };
        return decl;
    }
    
    std::vector<ast::Param> parse_param_list() {
        std::vector<ast::Param> params;
        if (check(TokenKind::RParen)) return params;

        bool ok = parse_comma_separated_recovering(
            [this]() -> std::optional<ast::Param> {
                bool is_mut = match(TokenKind::Mut);
                Token pname = consume_name("expected parameter name");
                consume(TokenKind::Colon, "expected ':' after parameter name");
                auto ptype = parse_type();
                if (!ptype) {
                    return std::nullopt;
                }
                return ast::Param{
                    .name = std::string(pname.text),
                    .type = std::move(*ptype),
                    .is_mut = is_mut
                };
            },
            [this]() { return check(TokenKind::RParen) || at_end(); },
            [&](ast::Param param) {
                params.push_back(std::move(param));
            },
            "expected ',' between parameters",
            [this]() { return looks_like_name_first_param(); });
        if (!ok) {
            return params;
        }

        return params;
    }

    void consume_member_separator() {
        match(TokenKind::Comma);
        match(TokenKind::Semicolon);
    }

    std::optional<ast::FieldDecl> parse_field_decl_flexible() {
        if (looks_like_typed_name_start()) {
            auto field_type = parse_type();
            if (!field_type) {
                return std::nullopt;
            }

            Token field_name = consume_name("expected field name");
            return ast::FieldDecl{
                .name = std::string(field_name.text),
                .type = std::move(*field_type)
            };
        }

        Token field_name = consume_name("expected field name");
        consume(TokenKind::Colon, "expected ':' after field name");
        auto field_type = parse_type();
        if (!field_type) {
            return std::nullopt;
        }

        return ast::FieldDecl{
            .name = std::string(field_name.text),
            .type = std::move(*field_type)
        };
    }

    std::optional<std::string> parse_string_or_name_value(std::string_view label) {
        if (check(TokenKind::StringLit)) {
            Token value = advance();
            auto* text = std::get_if<std::string>(&value.value);
            if (!text) {
                error(std::format("expected string value for {}", label));
                return std::nullopt;
            }
            return *text;
        }

        if (is_contextual_name_token(current(), NameMode::AllowTypeKeywords)) {
            return std::string(advance().text);
        }

        error(std::format("expected string or identifier for {}", label));
        return std::nullopt;
    }

    std::optional<bool> parse_bool_value(std::string_view label) {
        if (check(TokenKind::True)) {
            advance();
            return true;
        }
        if (check(TokenKind::False)) {
            advance();
            return false;
        }
        if (is_contextual_name_token(current(), NameMode::AllowTypeKeywords)) {
            auto text = std::string(advance().text);
            if (text == "true") {
                return true;
            }
            if (text == "false") {
                return false;
            }
            error(with_suggestion(
                std::format("invalid {} value: '{}', expected 'true' or 'false'", label, text),
                text,
                bool_value_candidates()));
            return std::nullopt;
        }

        error(std::format("expected 'true' or 'false' for {}", label));
        return std::nullopt;
    }

    std::optional<ast::HealingMode> parse_healing_mode_value() {
        auto value = parse_string_or_name_value("healing");
        if (!value) {
            return std::nullopt;
        }

        if (*value == "auto") {
            return ast::HealingMode::Auto;
        }
        if (*value == "freeze") {
            return ast::HealingMode::Freeze;
        }
        if (*value == "off") {
            return ast::HealingMode::Off;
        }

        error(with_suggestion(
            std::format("invalid healing mode: '{}', expected 'auto', 'freeze', or 'off'", *value),
            *value,
            healing_mode_candidates()));
        return std::nullopt;
    }

    std::optional<std::vector<std::string>> parse_string_list(std::string_view label) {
        consume(TokenKind::LBracket, std::format("expected '[' for {}", label));

        std::vector<std::string> values;
        while (!check(TokenKind::RBracket) && !at_end()) {
            if (!check_loop_safeguard("parse_string_list")) {
                break;
            }

            std::size_t before = pos_;
            Token value = consume(TokenKind::StringLit, std::format("expected {} string", label));
            auto* text = std::get_if<std::string>(&value.value);
            if (text) {
                values.push_back(*text);
            } else {
                error(std::format("expected string value for {}", label));
                return std::nullopt;
            }

            if (!check(TokenKind::RBracket)) {
                match(TokenKind::Comma);
            }

            if (pos_ == before && !at_end()) {
                advance();
            }
        }

        consume(TokenKind::RBracket, std::format("expected ']' after {}", label));
        return values;
    }

    // ========================================================================
    // OTHER DECLARATIONS
    // ========================================================================

    std::optional<ast::DeclPtr> parse_struct_decl() {
        Token name_tok = consume_name("expected struct name");
        consume(TokenKind::LBrace, "expected '{'");

        ast::FieldDeclList fields;
        parse_recovering_sequence(
            "parse_struct_decl",
            [this]() { return check(TokenKind::RBrace); },
            RecoveryMode::StructMember,
            [&]() {
                if (looks_like_typed_name_start() || is_contextual_name_token(current(), NameMode::AllowTypeKeywords)) {
                    if (auto field = parse_field_decl_flexible()) {
                        fields.push_back(std::move(*field));
                    }
                    consume_member_separator();
                    return true;
                }

                error("expected field in struct");
                return false;
            });

        consume(TokenKind::RBrace, "expected '}'");

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::StructDecl{
            .name = std::string(name_tok.text),
            .fields = std::move(fields)
        };
        return decl;
    }

    std::optional<ast::DeclPtr> parse_enum_decl() {
        Token name_tok = consume_name("expected enum name");
        consume(TokenKind::LBrace, "expected '{' after enum name");
        
        ast::EnumVariantList variants;
        std::int64_t next_value = 0;
        parse_recovering_sequence(
            "parse_enum_decl",
            [this]() { return check(TokenKind::RBrace); },
            RecoveryMode::EnumMember,
            [&]() {
                if (!is_contextual_name_token(current(), NameMode::AllowTypeKeywords)) {
                    error("expected variant name");
                    return false;
                }

                Token variant_name = consume_name("expected variant name");

                std::optional<std::int64_t> explicit_value;
                if (match(TokenKind::Assign)) {
                    Token val_tok = consume(TokenKind::IntLit, "expected integer value");
                    auto* val_ptr = std::get_if<std::int64_t>(&val_tok.value);
                    if (!val_ptr) {
                        error("expected integer value for enum variant");
                        return true;
                    }
                    explicit_value = *val_ptr;
                    next_value = *val_ptr + 1;
                } else {
                    explicit_value = next_value++;
                }

                variants.push_back(ast::EnumVariantDecl{
                    .name = std::string(variant_name.text),
                    .value = explicit_value
                });

                if (!check(TokenKind::RBrace) && !match(TokenKind::Comma)) {
                    error("expected ',' or '}' after enum variant");
                    return false;
                }

                return true;
            });
        
        consume(TokenKind::RBrace, "expected '}' after enum variants");
        
        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::EnumDecl{
            .name = std::string(name_tok.text),
            .variants = std::move(variants)
        };
        return decl;
    }

    std::optional<ast::DeclPtr> parse_const_decl() {
        Token name_tok = consume_name("expected constant name");
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

    std::optional<ast::DeclPtr> parse_static_decl(bool is_mut) {
        Token name_tok = consume_name("expected variable name");
        
        std::optional<Type> type;
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }
        
        ast::ExprPtr init;
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
        if (!type) {
            error("static declaration requires a type annotation");
            return std::nullopt;
        }
        sd.type = std::move(*type);
        sd.init = std::move(init);
        sd.is_mut = is_mut;
        decl->kind = std::move(sd);
        return decl;
    }

    std::optional<ast::DeclPtr> parse_import_decl() {
        Token path_tok = consume_name("expected module path");
        std::string path(path_tok.text);
        
        while (match(TokenKind::Dot)) {
            Token next = consume_name("expected identifier");
            path += ".";
            path += next.text;
        }

        std::optional<std::string> alias;
        if (match(TokenKind::As)) {
            Token alias_tok = consume_name("expected alias");
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

    std::optional<ast::DeclPtr> parse_type_alias_decl() {
        Token name_tok = consume_name("expected type alias name");
        consume(TokenKind::Assign, "expected '=' after type alias name");
        auto target = parse_type();
        if (!target) return std::nullopt;

        match(TokenKind::Semicolon);

        auto decl = std::make_unique<ast::Decl>();
        decl->kind = ast::TypeAliasDecl{
            .name = std::string(name_tok.text),
            .target = std::move(*target)
        };
        return decl;
    }

    std::optional<ast::ProjectDecl> parse_project_decl() {
        Token name_tok = consume_name("expected project name");
        consume(TokenKind::LBrace, "expected '{' after project name");
        
        ast::ProjectDecl proj;
        proj.name = std::string(name_tok.text);
        proj.mode = "dll";

        parse_recovering_sequence(
            "parse_project_decl",
            [this]() { return check(TokenKind::RBrace); },
            RecoveryMode::ProjectProperty,
            [&]() {
                std::string key;
                if (check(TokenKind::Include)) {
                    advance();
                    key = "include";
                } else if (is_contextual_name_token(current(), NameMode::AllowTypeKeywords)) {
                    key = std::string(advance().text);
                } else {
                    error("expected project property name");
                    return false;
                }

                consume(TokenKind::Colon, "expected ':' after property name");

                if (key == "entry") {
                    auto value = parse_string_or_name_value("entry");
                    if (!value) {
                        return false;
                    }
                    proj.entry = std::move(*value);
                } else if (key == "output") {
                    auto value = parse_string_or_name_value("output");
                    if (!value) {
                        return false;
                    }
                    proj.output = std::move(*value);
                } else if (key == "mode") {
                    auto value = parse_string_or_name_value("mode");
                    if (!value) {
                        return false;
                    }
                    if (*value != "dll" && *value != "exe") {
                        error(with_suggestion(
                            std::format("invalid project mode: '{}', expected 'dll' or 'exe'", *value),
                            *value,
                            project_mode_candidates()));
                    } else {
                        proj.mode = std::move(*value);
                    }
                } else if (key == "include") {
                    auto values = parse_string_list("include list");
                    if (!values) {
                        return false;
                    }
                    for (auto& value : *values) {
                        proj.includes.push_back(std::move(value));
                    }
                } else if (key == "debug") {
                    auto value = parse_bool_value("debug");
                    if (!value) {
                        return false;
                    }
                    proj.debug = *value;
                } else if (key == "healing") {
                    auto value = parse_healing_mode_value();
                    if (!value) {
                        return false;
                    }
                    proj.healing = *value;
                } else {
                    error_invalid_project_property(key);
                    return false;
                }

                match(TokenKind::Comma);
                return true;
            });

        consume(TokenKind::RBrace, "expected '}' after project declaration");
        return proj;
    }

    std::optional<ast::DeclPtr> parse_extern_decl() {
        if (match(TokenKind::Fn)) {
            auto sig = parse_signature_name_params_optional_return("expected function name");
            if (!sig) {
                return std::nullopt;
            }

            match(TokenKind::Semicolon);

            return make_fn_decl(
                std::move(sig->name),
                std::move(sig->params),
                std::move(sig->return_type),
                std::nullopt,
                {},
                ast::DeclAttrs{
                    .linkage = ast::Linkage::External
                });
        }
        if (match(TokenKind::Function)) {
            auto sig = parse_signature_type_name_params("expected function name");
            if (!sig) {
                return std::nullopt;
            }
            match(TokenKind::Semicolon);

            return make_fn_decl(
                std::move(sig->name),
                std::move(sig->params),
                std::move(sig->return_type),
                std::nullopt,
                {},
                ast::DeclAttrs{
                    .linkage = ast::Linkage::External
                });
        }
        error("expected 'fn' after 'extern'");
        return std::nullopt;
    }

    // ========================================================================
    // STATEMENTS
    // ========================================================================

    std::optional<ast::StmtPtr> parse_stmt() {
        switch (mode_) {
            case SyntaxMode::CStyle:
                return parse_stmt_in(StatementMode::CStyle);
            case SyntaxMode::English:
                return parse_stmt_in(StatementMode::English);
        }
        return std::nullopt;
    }

    std::optional<ast::StmtPtr> parse_stmt_in(StatementMode mode) {
        switch (mode) {
            case StatementMode::CStyle:
                return parse_stmt_c();
            case StatementMode::V2:
                return parse_stmt_v2();
            case StatementMode::English:
                return parse_stmt_english();
            case StatementMode::Mixed:
                return parse_stmt_mixed();
        }
        return std::nullopt;
    }

    ast::StmtSuite parse_block_stmts_mixed() {
        return parse_block_stmts(StatementMode::Mixed, "parse_block_stmts_mixed", true);
    }
    
    // figures out which syntax style this statement uses
    std::optional<ast::StmtPtr> parse_stmt_mixed() {
        if (check(TokenKind::Create)) return parse_stmt_english();
        if (check(TokenKind::Set)) return parse_stmt_english();
        if (check(TokenKind::Call)) return parse_stmt_english();
        if (looks_like_english_if_stmt()) return parse_stmt_english();
        if (looks_like_english_while_stmt()) return parse_stmt_english();
        return parse_stmt_v2();
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
            auto s = parse_loop_stmt(StatementMode::CStyle);
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
            auto stmts = parse_block_stmts(StatementMode::CStyle, "parse_block_stmts_cstyle");
            consume(TokenKind::RBrace, "expected '}'");
            auto stmt = std::make_unique<ast::Stmt>();
            stmt->kind = ast::BlockStmt{.block = ast::Block{.stmts = std::move(stmts)}};
            stmt->span.start = stmt_start;
            return stmt;
        }

        if (maybe_report_statement_keyword_typo()) {
            return std::nullopt;
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
        if (match(TokenKind::Create)) {
            if (check(TokenKind::Variable)) {
                advance(); // consume Variable
            } else {
                if (maybe_report_keyword_typo("variable", "expected 'variable' after 'create'")) {
                    return std::nullopt;
                }
                error("expected 'variable' after 'create'");
                return std::nullopt;
            }
            Token name = consume_name("expected variable name");
            consume_keyword_spelling(TokenKind::As, "as", "expected 'as'");
            auto type = parse_type();
            
            ast::ExprPtr init;
            if (match(TokenKind::With) && match(TokenKind::Value)) {
                if (auto expr = parse_expr()) {
                    init = std::move(*expr);
                }
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
            auto target = parse_expr();
            consume_keyword_spelling(TokenKind::To, "to", "expected 'to'");
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

        if (maybe_report_statement_keyword_typo()) {
            return std::nullopt;
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
        Token name = consume_name("expected variable name");
        
        std::optional<Type> type;
        if (match(TokenKind::Colon)) {
            type = parse_type();
        }

        ast::ExprPtr init;
        if (match(TokenKind::Assign)) {
            if (auto expr = parse_expr()) {
                init = std::move(*expr);
            }
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
        ast::ExprPtr value;
        if (!check(TokenKind::Semicolon) && !check(TokenKind::RBrace) && !at_end()) {
            if (auto expr = parse_expr()) {
                value = std::move(*expr);
            }
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
        
        auto then_block = parse_stmt_suite_or_single(
            StatementMode::CStyle,
            "parse_if_stmt_c_then",
            "expected '}' after if body");
        if (!then_block) return std::nullopt;

        std::optional<ast::StmtSuite> else_block;
        if (match(TokenKind::Else)) {
            else_block = parse_stmt_suite_or_single(
                StatementMode::CStyle,
                "parse_if_stmt_c_else",
                "expected '}' after else body");
            if (!else_block) return std::nullopt;
        }

        if (!cond) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::IfStmt{
            .condition = std::move(*cond),
            .then_block = std::move(*then_block),
            .else_block = std::move(else_block)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_if_stmt_english_chain(bool consume_end_if) {
        auto cond = parse_expr();
        consume_keyword_spelling(TokenKind::Then, "then", "expected 'then'");
        
        auto then_block = parse_stmt_suite_until_else_or_end(
            StatementMode::Mixed,
            "parse_if_stmt_english_then");

        std::optional<ast::StmtSuite> else_block;
        if (match(TokenKind::Else)) {
            if (match(TokenKind::If)) {
                auto nested = parse_if_stmt_english_chain(false);
                if (!nested) return std::nullopt;
                ast::StmtBlock else_stmts;
                else_stmts.push_back(std::move(*nested));
                else_block = ast::StmtSuite{std::move(else_stmts)};
            } else {
                else_block = parse_stmt_suite_until_end(
                    StatementMode::Mixed,
                    "parse_if_stmt_english_else");
            }
        }

        if (consume_end_if) {
            consume_keyword_spelling(TokenKind::End, "end", "expected 'end'");
            consume_optional_keyword_spelling(TokenKind::If, "if", "expected 'if' after 'end'");
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
        return parse_if_stmt_english_chain(true);
    }

    std::optional<ast::StmtPtr> parse_while_stmt_c() {
        consume(TokenKind::LParen, "expected '('");
        auto cond = parse_expr();
        if (!cond) return std::nullopt;
        consume(TokenKind::RParen, "expected ')'");
        auto body = parse_stmt_suite_or_single(
            StatementMode::CStyle,
            "parse_while_stmt_c_body",
            "expected '}' after while body");
        if (!body) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::WhileStmt{
            .condition = std::move(*cond),
            .body = std::move(*body)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_while_stmt_english() {
        auto cond = parse_expr();
        consume_keyword_spelling(TokenKind::Do, "do", "expected 'do'");
        
        auto body = parse_stmt_suite_until_end(StatementMode::Mixed, "parse_while_stmt_english");
        consume_keyword_spelling(TokenKind::End, "end", "expected 'end'");
        consume_optional_keyword_spelling(TokenKind::While, "while", "expected 'while' after 'end'");

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
        Token var = consume_name("expected variable");
        consume(TokenKind::In, "expected 'in'");
        auto iter = parse_expr();
        if (!iter) return std::nullopt;
        consume(TokenKind::RParen, "expected ')'");
        auto body = parse_stmt_suite_or_single(
            StatementMode::CStyle,
            "parse_for_stmt_c_body",
            "expected '}' after for body");
        if (!body) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ForStmt{
            .name = std::string(var.text),
            .iterable = std::move(*iter),
            .body = std::move(*body)
        };
        return stmt;
    }

    std::optional<ast::StmtPtr> parse_loop_stmt(StatementMode body_mode) {
        auto body = parse_stmt_suite_or_single(
            body_mode,
            body_mode == StatementMode::CStyle ? "parse_loop_stmt_c_body" : "parse_loop_stmt_v2_body",
            "expected '}' after loop body");
        if (!body) return std::nullopt;

        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::LoopStmt{.body = std::move(*body)};
        return stmt;
    }

    // ========================================================================
    // TYPES
    // ========================================================================

    std::optional<Type> parse_type() {
        if (match(TokenKind::Star)) {
            bool is_mut = match(TokenKind::Mut);
            auto inner = parse_type();
            if (!inner) return std::nullopt;
            return Type::make_ptr(std::move(*inner), is_mut);
        }

        std::optional<Type> base;

        if (match(TokenKind::Fn)) {
            consume(TokenKind::LParen, "expected '(' after fn in function type");

            std::vector<std::unique_ptr<Type>> params;
            if (!check(TokenKind::RParen)) {
                bool ok = parse_comma_separated_recovering(
                    [this]() -> std::optional<Type> {
                        return parse_type();
                    },
                    [this]() { return check(TokenKind::RParen) || at_end(); },
                    [&](Type param) {
                        params.push_back(std::make_unique<Type>(std::move(param)));
                    },
                    "expected ',' between function type parameters",
                    [this]() { return current().is_type() || is_contextual_name_token(current(), NameMode::AllowTypeKeywords); });
                if (!ok) return std::nullopt;
            }
            consume(TokenKind::RParen, "expected ')' after function type params");

            Type ret = Type::make_primitive(PrimitiveType::Void);
            if (match(TokenKind::Arrow) || match(TokenKind::Colon)) {
                auto ret_type = parse_type();
                if (!ret_type) return std::nullopt;
                ret = std::move(*ret_type);
            }

            Type t;
            t.kind = FunctionType{
                .params = std::move(params),
                .ret = std::make_unique<Type>(std::move(ret)),
                .is_variadic = false
            };
            base = std::move(t);
        } else if (match(TokenKind::LBracket)) {
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
            base = std::move(t);
        } else if (current().is_type()) {
            Token t = advance();
            PrimitiveType pt;
            switch (t.kind) {
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
            base = Type::make_primitive(pt);
        } else if (is_contextual_name_token(current(), true)) {
            Token name = advance();
            Type t;
            t.kind = std::string(name.text);
            base = std::move(t);
        } else {
            error("expected type");
            return std::nullopt;
        }

        while (match(TokenKind::LBracket)) {
            std::optional<std::size_t> size;
            if (match(TokenKind::Semicolon)) {
                Token n = consume(TokenKind::IntLit, "expected array size");
                if (auto* v = std::get_if<std::int64_t>(&n.value)) {
                    size = static_cast<std::size_t>(*v);
                }
            } else if (!check(TokenKind::RBracket)) {
                Token n = consume(TokenKind::IntLit, "expected array size or ']' after '['");
                if (auto* v = std::get_if<std::int64_t>(&n.value)) {
                    size = static_cast<std::size_t>(*v);
                }
            }
            consume(TokenKind::RBracket, "expected ']'");

            Type t;
            t.kind = ArrayType{
                .element = std::make_unique<Type>(std::move(*base)),
                .size = size
            };
            base = std::move(t);
        }

        return base;
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
            auto result = get_binary_op(current().kind);
            if (!result) break;
            auto [op, prec] = *result;
            if (prec < min_prec) break;
            
            advance();
            bool right_assoc = (op == ast::BinaryExpr::Op::Assign ||
                                op == ast::BinaryExpr::Op::AddAssign ||
                                op == ast::BinaryExpr::Op::SubAssign ||
                                op == ast::BinaryExpr::Op::MulAssign ||
                                op == ast::BinaryExpr::Op::DivAssign ||
                                op == ast::BinaryExpr::Op::ModAssign);
            auto rhs = parse_expr_prec(right_assoc ? prec : prec + 1);
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

    std::optional<std::pair<ast::BinaryExpr::Op, int>> get_binary_op(TokenKind kind) {
        using Op = ast::BinaryExpr::Op;
        switch (kind) {
            case TokenKind::OrOr:    return std::pair{Op::Or, 1};
            case TokenKind::AndAnd:  return std::pair{Op::And, 2};
            case TokenKind::Or:      return std::pair{Op::Or, 1};
            case TokenKind::And:     return std::pair{Op::And, 2};
            case TokenKind::Pipe:    return std::pair{Op::BitOr, 3};
            case TokenKind::Caret:   return std::pair{Op::BitXor, 4};
            case TokenKind::Ampersand: return std::pair{Op::BitAnd, 5};
            case TokenKind::Eq:      return std::pair{Op::Eq, 6};
            case TokenKind::Ne:      return std::pair{Op::Ne, 6};
            case TokenKind::Lt:      return std::pair{Op::Lt, 7};
            case TokenKind::Gt:      return std::pair{Op::Gt, 7};
            case TokenKind::Le:      return std::pair{Op::Le, 7};
            case TokenKind::Ge:      return std::pair{Op::Ge, 7};
            case TokenKind::Shl:     return std::pair{Op::Shl, 8};
            case TokenKind::Shr:     return std::pair{Op::Shr, 8};
            case TokenKind::Plus:    return std::pair{Op::Add, 9};
            case TokenKind::Minus:   return std::pair{Op::Sub, 9};
            case TokenKind::Star:    return std::pair{Op::Mul, 10};
            case TokenKind::Slash:   return std::pair{Op::Div, 10};
            case TokenKind::Percent: return std::pair{Op::Mod, 10};
            case TokenKind::Assign:  return std::pair{Op::Assign, 0};
            case TokenKind::PlusEq:  return std::pair{Op::AddAssign, 0};
            case TokenKind::MinusEq: return std::pair{Op::SubAssign, 0};
            case TokenKind::StarEq:  return std::pair{Op::MulAssign, 0};
            case TokenKind::SlashEq: return std::pair{Op::DivAssign, 0};
            case TokenKind::PercentEq: return std::pair{Op::ModAssign, 0};
            default: return std::nullopt;
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
                std::vector<ast::ExprPtr> args;
                if (!check(TokenKind::RParen)) {
                    bool ok = parse_comma_separated_recovering(
                        [this]() { return parse_expr(); },
                        [this]() { return check(TokenKind::RParen) || at_end(); },
                        [&](ast::ExprPtr arg) {
                            args.push_back(std::move(arg));
                        },
                        "expected ',' between arguments",
                        [this]() { return looks_like_expr_start(); });
                    if (!ok) return std::nullopt;
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
                Token field = consume_name("expected field name");
                auto access = std::make_unique<ast::Expr>();
                access->kind = ast::FieldExpr{
                    .base = std::move(*expr),
                    .field = std::string(field.text)
                };
                expr = std::move(access);
                if (looks_like_struct_literal_after()) {
                    auto qualified_name = expr_to_qualified_name(**expr);
                    if (qualified_name) {
                        return parse_struct_literal(*qualified_name);
                    }
                }
            }
            else if (match(TokenKind::As)) {
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
                auto inc = std::make_unique<ast::Expr>();
                inc->kind = ast::UnaryExpr{
                    .op = ast::UnaryExpr::Op::PostInc,
                    .operand = std::move(*expr)
                };
                expr = std::move(inc);
            }
            else if (match(TokenKind::MinusMinus)) {
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

    std::optional<ast::ExprPtr> parse_struct_literal(const std::string& struct_name) {
        consume(TokenKind::LBrace, "expected '{' after struct name");
        
        ast::FieldInitList fields;

        parse_recovering_sequence(
            "parse_struct_literal",
            [this]() { return check(TokenKind::RBrace); },
            RecoveryMode::StructLiteralField,
            [&]() {
                Token field_name = consume_name("expected field name");
                ast::ExprPtr value;
                if (match(TokenKind::Colon)) {
                    auto parsed_value = parse_expr();
                    if (!parsed_value) {
                        error("expected expression for field value");
                        return true;
                    }
                    value = std::move(*parsed_value);
                } else {
                    auto shorthand = std::make_unique<ast::Expr>();
                    shorthand->kind = ast::IdentExpr{.name = std::string(field_name.text)};
                    value = std::move(shorthand);
                }

                fields.push_back(ast::FieldInit{
                    .name = std::string(field_name.text),
                    .value = std::move(value)
                });

                if (!check(TokenKind::RBrace) && !match(TokenKind::Comma)) {
                    error("expected ',' or '}' after field value");
                    return false;
                }

                return true;
            });

        if (at_end()) {
            error("expected '}' after struct literal");
            return std::nullopt;
        }
        
        consume(TokenKind::RBrace, "expected '}' after struct literal");
        
        auto expr = std::make_unique<ast::Expr>();
        expr->kind = ast::StructExpr{
            .name = struct_name,
            .fields = std::move(fields)
        };
        return expr;
    }

    std::optional<std::string> expr_to_qualified_name(const ast::Expr& expr) {
        if (auto* ident = std::get_if<ast::IdentExpr>(&expr.kind)) {
            return ident->name;
        }
        if (auto* field = std::get_if<ast::FieldExpr>(&expr.kind)) {
            auto base = expr_to_qualified_name(*field->base);
            if (!base) {
                return std::nullopt;
            }
            return *base + "." + field->field;
        }
        return std::nullopt;
    }

    // spawn callable(args) -> SpawnExpr
    std::optional<ast::ExprPtr> parse_spawn_expr() {
        SourceLoc spawn_loc = previous().loc;
        auto target = parse_postfix();
        if (!target) return std::nullopt;

        auto* call = std::get_if<ast::CallExpr>(&(*target)->kind);
        if (!call) {
            error_at(spawn_loc, "spawn requires a function call like 'spawn worker()'");
            return std::nullopt;
        }
        
        auto expr = std::make_unique<ast::Expr>();
        expr->kind = ast::SpawnExpr{
            .callee = std::move(call->callee),
            .args = std::move(call->args)
        };
        return expr;
    }

    std::optional<ast::ExprPtr> parse_await_expr() {
        auto handle = parse_unary();
        if (!handle) return std::nullopt;
        
        auto expr = std::make_unique<ast::Expr>();
        expr->kind = ast::AwaitExpr{.handle = std::move(*handle)};
        return expr;
    }

    // parallel for i in range(start, end) { body }
    std::optional<ast::StmtPtr> parse_parallel_for() {
        consume(TokenKind::For, "expected 'for' after 'parallel'");
        Token var = consume_name("expected loop variable");
        consume(TokenKind::In, "expected 'in'");
        
        // expect range(start, end)
        Token range_tok = consume_name("expected 'range'");
        if (range_tok.text != "range") {
            error("parallel for requires range(start, end)");
            return std::nullopt;
        }
        
        consume(TokenKind::LParen, "expected '(' after 'range'");
        auto start = parse_expr();
        consume(TokenKind::Comma, "expected ',' between range arguments");
        auto end = parse_expr();
        consume(TokenKind::RParen, "expected ')' after range arguments");

        auto body = parse_stmt_suite_or_single(
            StatementMode::V2,
            "parse_parallel_for_body",
            "expected '}' after parallel for body");
        if (!body) return std::nullopt;
        
        if (!start || !end) return std::nullopt;
        
        auto stmt = std::make_unique<ast::Stmt>();
        stmt->kind = ast::ParallelForStmt{
            .name = std::string(var.text),
            .start = std::move(*start),
            .end = std::move(*end),
            .body = std::move(*body)
        };
        return stmt;
    }

    std::optional<ast::ExprPtr> parse_atomic_op(const std::string& name) {
        consume(TokenKind::LParen, std::format("expected '(' after '{}'", name));

        auto ptr = parse_expr();
        if (!ptr) return std::nullopt;

        if (name == "atomic_load") {
            consume(TokenKind::RParen, std::format("expected ')' after {} arguments", name));
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::AtomicLoadExpr{
                .ptr = std::move(*ptr)
            };
            return expr;
        }

        consume(TokenKind::Comma, std::format("expected ',' after first {} argument", name));
        auto first_value = parse_expr();
        if (!first_value) return std::nullopt;

        if (name == "atomic_add") {
            consume(TokenKind::RParen, std::format("expected ')' after {} arguments", name));
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::AtomicAddExpr{
                .ptr = std::move(*ptr),
                .value = std::move(*first_value)
            };
            return expr;
        }

        if (name == "atomic_store") {
            consume(TokenKind::RParen, std::format("expected ')' after {} arguments", name));
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::AtomicStoreExpr{
                .ptr = std::move(*ptr),
                .value = std::move(*first_value)
            };
            return expr;
        }

        consume(TokenKind::Comma, "expected ',' between atomic_cas expected and desired");
        auto desired = parse_expr();
        if (!desired) return std::nullopt;
        consume(TokenKind::RParen, std::format("expected ')' after {} arguments", name));

        auto expr = std::make_unique<ast::Expr>();
        expr->kind = ast::AtomicCompareExchangeExpr{
            .ptr = std::move(*ptr),
            .expected = std::move(*first_value),
            .desired = std::move(*desired)
        };
        return expr;
    }

    std::optional<ast::ExprPtr> parse_primary() {
        if (check(TokenKind::If)) {
            error("if expressions are not supported yet; use a statement form or bind the result manually");
            return std::nullopt;
        }
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
            bool is_f32 = !previous().text.empty() &&
                          (previous().text.back() == 'f' || previous().text.back() == 'F');
            expr->kind = ast::LiteralExpr{
                .value = val,
                .type = Type::make_primitive(is_f32 ? PrimitiveType::F32 : PrimitiveType::F64)
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
                .type = Type::make_primitive(PrimitiveType::Str)  // TODO proper hex type
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


        if (match(TokenKind::Spawn) || match(TokenKind::Thread)) {
            return parse_spawn_expr();
        }

        if (match(TokenKind::Await)) {
            return parse_await_expr();
        }

        if (is_contextual_name_token(current(), true)) {
            Token ident_tok = advance();
            std::string name(ident_tok.text);
            
            // atomic builtins need to be intercepted before normal call handling
            if (name == "atomic_add" || name == "atomic_cas" || 
                name == "atomic_load" || name == "atomic_store") {
                return parse_atomic_op(name);
            }
            
            // struct/class literal when a named type is followed by {
            if (looks_like_struct_literal_after()) {
                return parse_struct_literal(name);
            }
            
            auto expr = std::make_unique<ast::Expr>();
            expr->kind = ast::IdentExpr{.name = std::move(name)};
            return expr;
        }

        if (match(TokenKind::LParen)) {
            auto inner = parse_expr();
            consume(TokenKind::RParen, "expected ')'");
            return inner;
        }

        if (check(TokenKind::LBrace)) {
            error("block expressions are not supported yet; use a block statement instead");
            return std::nullopt;
        }

        if (match(TokenKind::LBracket)) {
            std::vector<ast::ExprPtr> elements;
            if (!check(TokenKind::RBracket)) {
                bool ok = parse_comma_separated_recovering(
                    [this]() { return parse_expr(); },
                    [this]() { return check(TokenKind::RBracket) || at_end(); },
                    [&](ast::ExprPtr elem) {
                        elements.push_back(std::move(elem));
                    },
                    "expected ',' between array elements",
                    [this]() { return looks_like_expr_start(); });
                if (!ok) return std::nullopt;
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
