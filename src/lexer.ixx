// lexer - supports c-style and english syntaxes

export module opus.lexer;

import opus.types;
import std;

export namespace opus {

enum class TokenKind : std::uint8_t {
    // Literals
    IntLit,
    FloatLit,
    StringLit,
    HexStringLit,   // hex"55 57 56 53..."
    CharLit,
    
    // Identifiers & Keywords
    Ident,
    
    // C-Style Keywords (v2.0 - C++/JS hybrid)
    Function,       // function keyword (JS style)
    Class,          // class keyword
    Struct,         // struct keyword
    Enum,           // enum keyword
    Project,        // project keyword (for project config)
    Include,        // include keyword (for project includes)
    Let,            // let (immutable variable)
    Var,            // var (mutable variable - JS style)
    Const,          // const
    Static,         // static
    If, Else, While, For, In, Loop,
    Return, Break, Continue,
    Import, Export, Extern,
    True, False,
    As, And, Or, Not,
    
    // Memory keywords (C/C++ power)
    New, Delete,        // C++ style
    Malloc, Free,       // C style
    Sizeof, Alignof,    // size operators
    Unsafe,             // unsafe block
    Manual,             // manual memory block
    
    // Concurrency
    Thread,             // thread function/block
    Spawn,              // spawn expr - fire off a new thread
    Await,              // await handle - block until thread done
    Parallel,           // parallel for - split work across cores
    
    // Type modifiers
    Auto,               // type inference (C++ style)
    Mut,                // mutable
    
    // Smart pointers
    Box, Rc, Weak,      // ownership types

    // Legacy keywords (backward compat)
    Fn,                 // fn (old Rust-style)

    // English Keywords (additional)
    Define, Returning, Variable, With, Value,
    Create, Set, To, End, Is, Than, Greater, Less,
    Equal, Begin, Then, Do, Until, Repeat, Call,

    
    // Type names - C++/JS friendly
    TypeVoid, TypeBool, 
    TypeInt, TypeLong, TypeShort, TypeByte,          // signed
    TypeUint, TypeUlong, TypeUshort, TypeUbyte,      // unsigned
    TypeFloat, TypeDouble,                            // floating
    TypeString, TypeChar, TypePtr,                    // other
    TypeAuto,                                         // inferred
    // Legacy type names (backward compat)
    TypeI8, TypeI16, TypeI32, TypeI64, TypeI128,
    TypeU8, TypeU16, TypeU32, TypeU64, TypeU128, 
    TypeF32, TypeF64, TypeStr,

    // Operators
    Plus, Minus, Star, Slash, Percent,
    Ampersand, Pipe, Caret, Tilde,
    Shl, Shr,
    Eq, Ne, Lt, Gt, Le, Ge,
    AndAnd, OrOr, Bang,
    Assign, PlusEq, MinusEq, StarEq, SlashEq,
    PlusPlus, MinusMinus,   // ++ and --
    
    // Delimiters
    LParen, RParen,
    LBrace, RBrace,
    LBracket, RBracket,
    Comma, Colon, Semicolon, Dot,
    Arrow,      // ->
    FatArrow,   // =>
    ColonColon, // ::
    At,         // @ (decorators/annotations)
    Hash,       // # (for directives)
    Question,   // ? (nullable types)
    
    // Special
    Newline,    // Significant in English syntax
    Eof,
    Error,
};

struct Token {
    TokenKind kind;
    std::string_view text;
    SourceLoc loc;
    
    // parsed literal value if applicable
    std::variant<
        std::monostate,
        std::int64_t,
        std::uint64_t,
        double,
        std::string
    > value;

    bool is(TokenKind k) const { return kind == k; }
    bool is_keyword() const { return kind >= TokenKind::Function && kind <= TokenKind::Call; }
    bool is_literal() const { return kind >= TokenKind::IntLit && kind <= TokenKind::CharLit; }
    bool is_type() const { return kind >= TokenKind::TypeVoid && kind <= TokenKind::TypeStr; }
    bool is_operator() const { return kind >= TokenKind::Plus && kind <= TokenKind::MinusMinus; }
};

// ============================================================================
// LEXER
// ============================================================================

class Lexer {
public:
    // mixed mode: auto-detect syntax per token
    Lexer(std::string_view source, std::string_view filename)
        : source_(source)
        , filename_(filename)
        , mode_(SyntaxMode::CStyle)  // Default, but we detect per-token
        , mixed_mode_(true)
        , pos_(0)
        , loc_{.line = 1, .column = 1, .offset = 0, .file = filename}
    {}

    Lexer(std::string_view source, std::string_view filename, SyntaxMode mode)
        : source_(source)
        , filename_(filename)
        , mode_(mode)
        , mixed_mode_(false)
        , pos_(0)
        , loc_{.line = 1, .column = 1, .offset = 0, .file = filename}
    {}

    Token next() {
        skip_whitespace();
        
        if (at_end()) {
            return make_token(TokenKind::Eof, "");
        }

        SourceLoc start_loc = loc_;
        char c = peek();

        if (std::isdigit(c) || (c == '.' && std::isdigit(peek(1)))) {
            return lex_number();
        }

        if (c == '"') {
            return lex_string();
        }

        if (c == '\'') {
            return lex_char();
        }

        if (std::isalpha(c) || c == '_') {
            return lex_ident();
        }

        return lex_operator();
    }

    std::vector<Token> tokenize_all() {
        std::vector<Token> tokens;
        while (true) {
            Token tok = next();
            tokens.push_back(tok);
            if (tok.kind == TokenKind::Eof || tok.kind == TokenKind::Error) {
                break;
            }
        }
        return tokens;
    }

    SyntaxMode mode() const { return mode_; }
    
    static SyntaxMode detect_syntax(std::string_view source) {
        std::size_t i = 0;
        while (i < source.size() && std::isspace(source[i])) i++;
        
        if (i >= source.size()) return SyntaxMode::CStyle;

        std::string_view rest = source.substr(i);
        
        if (rest.starts_with("define ") || 
            rest.starts_with("create ") ||
            rest.starts_with("begin ")) {
            return SyntaxMode::English;
        }

        return SyntaxMode::CStyle;
    }

private:
    std::string_view source_;
    std::string_view filename_;
    SyntaxMode mode_;
    bool mixed_mode_;
    std::size_t pos_;
    SourceLoc loc_;

    bool at_end() const { return pos_ >= source_.size(); }
    
    char peek(std::size_t offset = 0) const {
        if (pos_ + offset >= source_.size()) return '\0';
        return source_[pos_ + offset];
    }

    char advance() {
        char c = source_[pos_++];
        loc_.offset = static_cast<std::uint32_t>(pos_);
        if (c == '\n') {
            loc_.line++;
            loc_.column = 1;
        } else {
            loc_.column++;
        }
        return c;
    }

    void skip_whitespace() {
        while (!at_end()) {
            char c = peek();
            
            // TODO: english mode should treat newlines as significant
            if (c == '\n' && mode_ == SyntaxMode::English) {
            }

            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else if (c == '/' && peek(1) == '/') {
                while (!at_end() && peek() != '\n') advance();
            } else if (c == '/' && peek(1) == '*') {
                advance(); advance();
                while (!at_end() && !(peek() == '*' && peek(1) == '/')) {
                    advance();
                }
                if (!at_end()) { advance(); advance(); }
            } else {
                break;
            }
        }
    }

    Token make_token(TokenKind kind, std::string_view text) {
        Token tok;
        tok.kind = kind;
        tok.text = text;
        tok.loc = loc_;
        tok.value = std::monostate{};
        return tok;
    }

    Token lex_number() {
        SourceLoc start = loc_;
        std::size_t start_pos = pos_;
        bool is_float = false;
        bool is_hex = false;
        bool is_bin = false;

        if (peek() == '0') {
            if (peek(1) == 'x' || peek(1) == 'X') {
                is_hex = true;
                advance(); advance();
            } else if (peek(1) == 'b' || peek(1) == 'B') {
                is_bin = true;
                advance(); advance();
            }
        }

        if (is_hex) {
            while (std::isxdigit(peek())) advance();
        } else if (is_bin) {
            while (peek() == '0' || peek() == '1') advance();
        } else {
            while (std::isdigit(peek())) advance();
            
            if (peek() == '.' && std::isdigit(peek(1))) {
                is_float = true;
                advance();
                while (std::isdigit(peek())) advance();
            }

            if (peek() == 'e' || peek() == 'E') {
                is_float = true;
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (std::isdigit(peek())) advance();
            }
        }

        // TODO: properly parse type suffixes like i32, u64, f32
        while (std::isalnum(peek())) advance();

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        Token tok = make_token(is_float ? TokenKind::FloatLit : TokenKind::IntLit, text);
        tok.loc = start;

        try {
            if (is_float) {
                tok.value = std::stod(std::string(text));
            } else if (is_hex) {
                tok.value = static_cast<std::int64_t>(std::stoull(std::string(text), nullptr, 16));
            } else if (is_bin) {
                tok.value = static_cast<std::int64_t>(std::stoull(std::string(text.substr(2)), nullptr, 2));
            } else {
                tok.value = std::stoll(std::string(text));
            }
        } catch (...) {
            tok.kind = TokenKind::Error;
        }

        return tok;
    }

    Token lex_string() {
        SourceLoc start = loc_;
        std::size_t start_pos = pos_;
        advance();

        std::string value;
        while (!at_end() && peek() != '"') {
            if (peek() == '\\') {
                advance();
                switch (peek()) {
                    case 'n':  value += '\n'; break;
                    case 't':  value += '\t'; break;
                    case 'r':  value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"':  value += '"';  break;
                    case '0':  value += '\0'; break;
                    default:   value += peek(); break;
                }
                advance();
            } else {
                value += advance();
            }
        }

        if (at_end()) {
            return make_token(TokenKind::Error, "unterminated string");
        }
        advance();

        Token tok = make_token(TokenKind::StringLit, source_.substr(start_pos, pos_ - start_pos));
        tok.loc = start;
        tok.value = std::move(value);
        return tok;
    }

    Token lex_char() {
        SourceLoc start = loc_;
        std::size_t start_pos = pos_;
        advance();

        char value = 0;
        if (peek() == '\\') {
            advance();
            switch (peek()) {
                case 'n':  value = '\n'; break;
                case 't':  value = '\t'; break;
                case 'r':  value = '\r'; break;
                case '\\': value = '\\'; break;
                case '\'': value = '\''; break;
                case '0':  value = '\0'; break;
                default:   value = peek(); break;
            }
            advance();
        } else {
            value = advance();
        }

        if (peek() != '\'') {
            return make_token(TokenKind::Error, "unterminated char");
        }
        advance();

        Token tok = make_token(TokenKind::CharLit, source_.substr(start_pos, pos_ - start_pos));
        tok.loc = start;
        tok.value = static_cast<std::int64_t>(value);
        return tok;
    }

    Token lex_hex_string() {
        SourceLoc start = loc_;
        std::size_t start_pos = pos_;
        advance();
        
        std::vector<std::uint8_t> bytes;
        
        while (!at_end() && peek() != '\"') {
            char c = peek();
            
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                advance();
                continue;
            }
            
            if (std::isxdigit(c)) {
                char hex[3] = {c, 0, 0};
                advance();
                
                if (!at_end() && std::isxdigit(peek())) {
                    hex[1] = peek();
                    advance();
                }
                
                try {
                    std::uint8_t byte = static_cast<std::uint8_t>(std::stoul(hex, nullptr, 16));
                    bytes.push_back(byte);
                } catch (...) {
                    return make_token(TokenKind::Error, "invalid hex in hex string");
                }
            } else {
                return make_token(TokenKind::Error, "invalid character in hex string");
            }
        }
        
        if (at_end()) {
            return make_token(TokenKind::Error, "unterminated hex string");
        }
        advance();
        
        // bytes stored as raw string, codegen handles the interpretation
        std::string hex_data;
        for (std::uint8_t b : bytes) {
            hex_data += static_cast<char>(b);
        }
        
        Token tok = make_token(TokenKind::HexStringLit, source_.substr(start_pos, pos_ - start_pos));
        tok.loc = start;
        tok.value = std::move(hex_data);
        return tok;
    }

    Token lex_ident() {
        SourceLoc start = loc_;
        std::size_t start_pos = pos_;
        
        while (std::isalnum(peek()) || peek() == '_') {
            advance();
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        
        // hex"55 57 56..." syntax
        if (text == "hex" && peek() == '\"') {
            return lex_hex_string();
        }
        
        TokenKind kind = keyword_or_ident(text);
        
        Token tok = make_token(kind, text);
        tok.loc = start;
        return tok;
    }

    TokenKind keyword_or_ident(std::string_view text) {
        // C-style keywords (v2.0 - C++/JS hybrid)
        static const std::unordered_map<std::string_view, TokenKind> c_keywords = {
            // Function/class keywords - ALL ALIASES WORK
            {"function", TokenKind::Function},
            {"func", TokenKind::Function},      // Alias: Go style
            {"fn", TokenKind::Function},        // Alias: Rust style
            {"class", TokenKind::Class},
            {"struct", TokenKind::Struct},
            {"structure", TokenKind::Struct},   // Alias: verbose
            {"enum", TokenKind::Enum},
            {"project", TokenKind::Project},
            {"include", TokenKind::Include},
            
            // Variable keywords - ALL ALIASES WORK
            {"let", TokenKind::Let},
            {"var", TokenKind::Var},
            {"const", TokenKind::Const},
            {"static", TokenKind::Static},
            {"auto", TokenKind::Auto},
            {"mut", TokenKind::Mut},
            
            // Control flow - ALL ALIASES WORK
            {"if", TokenKind::If},
            {"else", TokenKind::Else},
            {"elif", TokenKind::Else},          // Alias: Python style (parser handles)
            {"while", TokenKind::While},
            {"for", TokenKind::For},
            {"in", TokenKind::In},
            {"loop", TokenKind::Loop},
            {"return", TokenKind::Return},
            {"ret", TokenKind::Return},         // Alias: short form
            {"break", TokenKind::Break},
            {"continue", TokenKind::Continue},
            {"cont", TokenKind::Continue},      // Alias: short form
            
            // Modules
            {"import", TokenKind::Import},
            {"export", TokenKind::Export},
            {"extern", TokenKind::Extern},
            {"external", TokenKind::Extern},    // Alias: verbose
            
            // Literals - ALL ALIASES WORK
            {"true", TokenKind::True},
            {"false", TokenKind::False},
            {"yes", TokenKind::True},           // Alias: English
            {"no", TokenKind::False},           // Alias: English
            
            // Operators as keywords
            {"as", TokenKind::As},
            {"and", TokenKind::And},
            {"or", TokenKind::Or},
            {"not", TokenKind::Not},
            
            // Memory keywords
            {"new", TokenKind::New},
            {"delete", TokenKind::Delete},
            {"malloc", TokenKind::Malloc},
            {"alloc", TokenKind::Malloc},       // Alias: short form
            {"free", TokenKind::Free},
            {"sizeof", TokenKind::Sizeof},
            {"alignof", TokenKind::Alignof},
            {"unsafe", TokenKind::Unsafe},
            {"manual", TokenKind::Manual},
            
            // Concurrency
            {"thread", TokenKind::Thread},
            {"async", TokenKind::Thread},       // Alias: JS style
            {"spawn", TokenKind::Spawn},
            {"await", TokenKind::Await},
            {"parallel", TokenKind::Parallel},
            
            // Smart pointers
            {"Box", TokenKind::Box},
            {"Rc", TokenKind::Rc},
            {"Weak", TokenKind::Weak},
        };

        // English keywords
        static const std::unordered_map<std::string_view, TokenKind> en_keywords = {
            {"define", TokenKind::Define},
            {"returning", TokenKind::Returning},
            {"variable", TokenKind::Variable},
            {"with", TokenKind::With},
            {"value", TokenKind::Value},
            {"create", TokenKind::Create},
            {"set", TokenKind::Set},
            {"to", TokenKind::To},
            {"end", TokenKind::End},
            {"is", TokenKind::Is},
            {"than", TokenKind::Than},
            {"greater", TokenKind::Greater},
            {"less", TokenKind::Less},
            {"equal", TokenKind::Equal},
            {"begin", TokenKind::Begin},
            {"then", TokenKind::Then},
            {"do", TokenKind::Do},
            {"until", TokenKind::Until},
            {"repeat", TokenKind::Repeat},
            {"call", TokenKind::Call},
        };

        // Type names - Minimal set to avoid variable name conflicts
        // Use explicit types (i32, f64) over ambiguous words (int, double)
        static const std::unordered_map<std::string_view, TokenKind> types = {
            // Core types only
            {"void", TokenKind::TypeVoid},
            {"bool", TokenKind::TypeBool},
            {"int", TokenKind::TypeInt},
            {"uint", TokenKind::TypeUint},
            {"str", TokenKind::TypeStr},
            {"ptr", TokenKind::TypePtr},
            
            // Rust-style explicit types (preferred - clear and unambiguous)
            {"i8", TokenKind::TypeI8},
            {"i16", TokenKind::TypeI16},
            {"i32", TokenKind::TypeI32},
            {"i64", TokenKind::TypeI64},
            {"i128", TokenKind::TypeI128},
            {"u8", TokenKind::TypeU8},
            {"u16", TokenKind::TypeU16},
            {"u32", TokenKind::TypeU32},
            {"u64", TokenKind::TypeU64},
            {"u128", TokenKind::TypeU128},
            {"f32", TokenKind::TypeF32},
            {"f64", TokenKind::TypeF64},
            
            // Size types
            {"usize", TokenKind::TypeU64},
            {"isize", TokenKind::TypeI64},
        };

        // types are shared across both syntaxes so check them first
        if (auto it = types.find(text); it != types.end()) {
            return it->second;
        }

        // c keywords are the hot path
        if (auto it = c_keywords.find(text); it != c_keywords.end()) {
            return it->second;
        }

        if (mixed_mode_ || mode_ == SyntaxMode::English) {
            if (auto it = en_keywords.find(text); it != en_keywords.end()) {
                return it->second;
            }
        }

        return TokenKind::Ident;
    }

    Token lex_operator() {
        SourceLoc start = loc_;
        char c = advance();

        auto single = [&](TokenKind k) {
            Token tok = make_token(k, source_.substr(pos_ - 1, 1));
            tok.loc = start;
            return tok;
        };

        auto double_or_single = [&](char next, TokenKind double_kind, TokenKind single_kind) {
            if (peek() == next) {
                advance();
                Token tok = make_token(double_kind, source_.substr(pos_ - 2, 2));
                tok.loc = start;
                return tok;
            }
            Token tok = make_token(single_kind, source_.substr(pos_ - 1, 1));
            tok.loc = start;
            return tok;
        };

        switch (c) {
            case '(': return single(TokenKind::LParen);
            case ')': return single(TokenKind::RParen);
            case '{': return single(TokenKind::LBrace);
            case '}': return single(TokenKind::RBrace);
            case '[': return single(TokenKind::LBracket);
            case ']': return single(TokenKind::RBracket);
            case ',': return single(TokenKind::Comma);
            case ';': return single(TokenKind::Semicolon);
            case '.': return single(TokenKind::Dot);
            case '@': return single(TokenKind::At);
            case '#': return single(TokenKind::Hash);
            case '~': return single(TokenKind::Tilde);
            case '^': return single(TokenKind::Caret);

            case '+': {
                if (peek() == '=') { 
                    advance(); 
                    Token tok = make_token(TokenKind::PlusEq, "+=");
                    tok.loc = start;
                    return tok;
                }
                if (peek() == '+') { 
                    advance(); 
                    Token tok = make_token(TokenKind::PlusPlus, "++");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Plus);
            }

            case '-': {
                if (peek() == '>') { 
                    advance(); 
                    Token tok = make_token(TokenKind::Arrow, "->");
                    tok.loc = start;
                    return tok;
                }
                if (peek() == '=') { 
                    advance(); 
                    Token tok = make_token(TokenKind::MinusEq, "-=");
                    tok.loc = start;
                    return tok;
                }
                if (peek() == '-') { 
                    advance(); 
                    Token tok = make_token(TokenKind::MinusMinus, "--");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Minus);
            }

            case '*': {
                if (peek() == '=') { 
                    advance(); 
                    Token tok = make_token(TokenKind::StarEq, "*=");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Star);
            }

            case '/': {
                if (peek() == '=') { 
                    advance(); 
                    Token tok = make_token(TokenKind::SlashEq, "/=");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Slash);
            }

            case '%': return single(TokenKind::Percent);

            case '&': {
                if (peek() == '&') { 
                    advance(); 
                    Token tok = make_token(TokenKind::AndAnd, "&&");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Ampersand);
            }

            case '|': {
                if (peek() == '|') { 
                    advance(); 
                    Token tok = make_token(TokenKind::OrOr, "||");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Pipe);
            }

            case '!': {
                if (peek() == '=') { 
                    advance(); 
                    Token tok = make_token(TokenKind::Ne, "!=");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Bang);
            }

            case '=': {
                if (peek() == '=') { 
                    advance(); 
                    Token tok = make_token(TokenKind::Eq, "==");
                    tok.loc = start;
                    return tok;
                }
                if (peek() == '>') { 
                    advance(); 
                    Token tok = make_token(TokenKind::FatArrow, "=>");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Assign);
            }

            case '<': {
                if (peek() == '<') { 
                    advance(); 
                    Token tok = make_token(TokenKind::Shl, "<<");
                    tok.loc = start;
                    return tok;
                }
                if (peek() == '=') { 
                    advance(); 
                    Token tok = make_token(TokenKind::Le, "<=");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Lt);
            }

            case '>': {
                if (peek() == '>') { 
                    advance(); 
                    Token tok = make_token(TokenKind::Shr, ">>");
                    tok.loc = start;
                    return tok;
                }
                if (peek() == '=') { 
                    advance(); 
                    Token tok = make_token(TokenKind::Ge, ">=");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Gt);
            }

            case ':': {
                if (peek() == ':') { 
                    advance(); 
                    Token tok = make_token(TokenKind::ColonColon, "::");
                    tok.loc = start;
                    return tok;
                }
                return single(TokenKind::Colon);
            }

            default: {
                Token tok = make_token(TokenKind::Error, source_.substr(pos_ - 1, 1));
                tok.loc = start;
                return tok;
            }
        }
    }
};

// ============================================================================
// TOKEN UTILITIES
// ============================================================================

constexpr std::string_view token_kind_name(TokenKind k) {
    switch (k) {
        case TokenKind::IntLit:    return "integer";
        case TokenKind::FloatLit:  return "float";
        case TokenKind::StringLit: return "string";
        case TokenKind::HexStringLit: return "hex string";
        case TokenKind::CharLit:   return "char";
        case TokenKind::Ident:     return "identifier";
        case TokenKind::Fn:        return "fn";
        case TokenKind::Let:       return "let";
        case TokenKind::Return:    return "return";
        case TokenKind::Eof:       return "end of file";
        case TokenKind::Error:     return "error";
        default:                   return "token";
    }
}

} // namespace opus

