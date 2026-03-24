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
    Assign, PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
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
        , loc_{.line = 1, .column = 1, .offset = 0, .file = std::string(filename)}
    {}

    Lexer(std::string_view source, std::string_view filename, SyntaxMode mode)
        : source_(source)
        , filename_(filename)
        , mode_(mode)
        , mixed_mode_(false)
        , pos_(0)
        , loc_{.line = 1, .column = 1, .offset = 0, .file = std::string(filename)}
    {}

    Token next() {
        if (auto err = skip_whitespace()) {
            return *err;
        }
        
        if (at_end()) {
            return make_token(TokenKind::Eof, "", loc_);
        }

        char c = peek();

        if (is_digit_char(c) || (c == '.' && is_digit_char(peek(1)))) {
            return lex_number();
        }

        if (c == '"') {
            return lex_string();
        }

        if (c == '\'') {
            return lex_char();
        }

        if (is_alpha_char(c) || c == '_') {
            return lex_ident();
        }

        return lex_operator();
    }

    std::vector<Token> tokenize_all() {
        std::vector<Token> tokens;
        tokens.reserve(source_.size() / 4);
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
        while (i < source.size() && std::isspace(static_cast<unsigned char>(source[i]))) i++;
        
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

    [[nodiscard]] static unsigned char uchar(char c) {
        return static_cast<unsigned char>(c);
    }

    [[nodiscard]] static bool is_digit_char(char c) {
        return std::isdigit(uchar(c)) != 0;
    }

    [[nodiscard]] static bool is_alpha_char(char c) {
        return std::isalpha(uchar(c)) != 0;
    }

    [[nodiscard]] static bool is_alnum_char(char c) {
        return std::isalnum(uchar(c)) != 0;
    }

    [[nodiscard]] static bool is_xdigit_char(char c) {
        return std::isxdigit(uchar(c)) != 0;
    }

    bool at_end() const { return pos_ >= source_.size(); }
    
    char peek(std::size_t offset = 0) const {
        if (pos_ + offset >= source_.size()) return '\0';
        return source_[pos_ + offset];
    }

    char advance() {
        loc_.offset = static_cast<std::uint32_t>(pos_);
        char c = source_[pos_++];
        if (c == '\n') {
            loc_.line++;
            loc_.column = 1;
        } else {
            loc_.column++;
        }
        return c;
    }

    std::optional<Token> skip_whitespace() {
        while (!at_end()) {
            char c = peek();
            
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else if (c == '/' && peek(1) == '/') {
                while (!at_end() && peek() != '\n') advance();
            } else if (c == '/' && peek(1) == '*') {
                SourceLoc start = loc_;
                advance(); advance();
                while (!at_end() && !(peek() == '*' && peek(1) == '/')) {
                    advance();
                }
                if (at_end()) {
                    return make_token(TokenKind::Error, "unterminated block comment", start);
                }
                advance(); advance();
            } else {
                break;
            }
        }
        return std::nullopt;
    }

    Token make_token(TokenKind kind, std::string_view text, SourceLoc start) {
        Token tok;
        tok.kind = kind;
        tok.text = text;
        tok.loc = start;
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
            while (is_xdigit_char(peek())) advance();
        } else if (is_bin) {
            while (peek() == '0' || peek() == '1') advance();
        } else {
            while (is_digit_char(peek())) advance();
            
            if (peek() == '.' && is_digit_char(peek(1))) {
                is_float = true;
                advance();
                while (is_digit_char(peek())) advance();
            }

            if (peek() == 'e' || peek() == 'E') {
                is_float = true;
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (is_digit_char(peek())) advance();
            }
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        Token tok = make_token(is_float ? TokenKind::FloatLit : TokenKind::IntLit, text, start);
        auto consumed_all = [&](const char* begin, const char* end, const auto& parsed) {
            return parsed.ec == std::errc{} && parsed.ptr == end;
        };

        // parse numeric value with from_chars (no locale, no alloc)
        if (is_float) {
            double dval = 0.0;
            auto parsed = std::from_chars(text.data(), text.data() + text.size(), dval);
            if (consumed_all(text.data(), text.data() + text.size(), parsed)) {
                tok.value = dval;
            } else {
                tok.kind = TokenKind::Error;
                tok.text = "invalid float literal";
            }
        } else if (is_hex) {
            // skip the 0x prefix for from_chars
            std::string_view digits = text.substr(2);
            std::uint64_t uval = 0;
            auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), uval, 16);
            if (!digits.empty() && consumed_all(digits.data(), digits.data() + digits.size(), parsed)) {
                tok.value = static_cast<std::int64_t>(uval);
            } else {
                tok.kind = TokenKind::Error;
                tok.text = "invalid hex literal";
            }
        } else if (is_bin) {
            // skip the 0b prefix for from_chars
            std::string_view digits = text.substr(2);
            std::uint64_t uval = 0;
            auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), uval, 2);
            if (!digits.empty() && consumed_all(digits.data(), digits.data() + digits.size(), parsed)) {
                tok.value = static_cast<std::int64_t>(uval);
            } else {
                tok.kind = TokenKind::Error;
                tok.text = "invalid binary literal";
            }
        } else {
            std::int64_t ival = 0;
            auto parsed = std::from_chars(text.data(), text.data() + text.size(), ival);
            if (consumed_all(text.data(), text.data() + text.size(), parsed)) {
                tok.value = ival;
            } else {
                tok.kind = TokenKind::Error;
                tok.text = "invalid integer literal";
            }
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
                if (at_end()) {
                    return make_token(TokenKind::Error, "unterminated string", start);
                }
                switch (peek()) {
                    case 'n':  value += '\n'; break;
                    case 't':  value += '\t'; break;
                    case 'r':  value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"':  value += '"';  break;
                    case '0':  value += '\0'; break;
                    default:
                        return make_token(TokenKind::Error, "unknown escape sequence in string", start);
                }
                advance();
            } else {
                value += advance();
            }
        }

        if (at_end()) {
            return make_token(TokenKind::Error, "unterminated string", start);
        }
        advance();

        Token tok = make_token(TokenKind::StringLit, source_.substr(start_pos, pos_ - start_pos), start);
        tok.value = std::move(value);
        return tok;
    }

    Token lex_char() {
        SourceLoc start = loc_;
        std::size_t start_pos = pos_;
        advance();

        if (at_end()) {
            return make_token(TokenKind::Error, "unterminated char", start);
        }

        std::uint32_t value = 0;
        if (peek() == '\\') {
            advance();
            if (at_end()) {
                return make_token(TokenKind::Error, "unterminated char", start);
            }
            switch (peek()) {
                case 'n':  value = '\n'; break;
                case 't':  value = '\t'; break;
                case 'r':  value = '\r'; break;
                case '\\': value = '\\'; break;
                case '\'': value = '\''; break;
                case '0':  value = '\0'; break;
                default:
                    return make_token(TokenKind::Error, "unknown escape sequence in char", start);
            }
            advance();
        } else {
            auto decoded = advance_utf8_codepoint();
            if (!decoded) {
                return make_token(TokenKind::Error, "invalid utf-8 char literal", start);
            }
            value = *decoded;
        }

        if (peek() != '\'') {
            return make_token(TokenKind::Error, "unterminated char", start);
        }
        advance();

        Token tok = make_token(TokenKind::CharLit, source_.substr(start_pos, pos_ - start_pos), start);
        tok.value = static_cast<std::int64_t>(value);
        return tok;
    }

    Token lex_hex_string(SourceLoc start, std::size_t start_pos) {
        advance();
        
        std::vector<std::uint8_t> bytes;
        std::optional<char> pending_nibble;
        
        while (!at_end() && peek() != '\"') {
            char c = peek();
            
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                advance();
                continue;
            }
            
            if (is_xdigit_char(c)) {
                if (!pending_nibble) {
                    pending_nibble = c;
                    advance();
                } else {
                    char hex[3] = {*pending_nibble, c, 0};
                    advance();
                    try {
                        std::uint8_t byte = static_cast<std::uint8_t>(std::stoul(hex, nullptr, 16));
                        bytes.push_back(byte);
                    } catch (...) {
                        return make_token(TokenKind::Error, "invalid hex in hex string", start);
                    }
                    pending_nibble.reset();
                }
            } else {
                return make_token(TokenKind::Error, "invalid character in hex string", start);
            }
        }

        if (pending_nibble) {
            return make_token(TokenKind::Error, "hex string must contain whole bytes", start);
        }
        
        if (at_end()) {
            return make_token(TokenKind::Error, "unterminated hex string", start);
        }
        advance();
        
        // bytes stored as raw string, codegen handles the interpretation
        std::string hex_data;
        for (std::uint8_t b : bytes) {
            hex_data += static_cast<char>(b);
        }
        
        Token tok = make_token(TokenKind::HexStringLit, source_.substr(start_pos, pos_ - start_pos), start);
        tok.value = std::move(hex_data);
        return tok;
    }

    std::optional<std::uint32_t> advance_utf8_codepoint() {
        if (at_end()) return std::nullopt;

        auto read_cont = [this]() -> std::optional<std::uint8_t> {
            if (at_end()) return std::nullopt;
            unsigned char b = static_cast<unsigned char>(peek());
            if ((b & 0xC0) != 0x80) return std::nullopt;
            advance();
            return static_cast<std::uint8_t>(b & 0x3F);
        };

        unsigned char first = static_cast<unsigned char>(peek());
        if (first < 0x80) {
            advance();
            return first;
        }

        if ((first & 0xE0) == 0xC0) {
            advance();
            auto c1 = read_cont();
            if (!c1) return std::nullopt;
            std::uint32_t cp = (static_cast<std::uint32_t>(first & 0x1F) << 6) | *c1;
            if (cp < 0x80) return std::nullopt;
            return cp;
        }

        if ((first & 0xF0) == 0xE0) {
            advance();
            auto c1 = read_cont();
            auto c2 = read_cont();
            if (!c1 || !c2) return std::nullopt;
            std::uint32_t cp = (static_cast<std::uint32_t>(first & 0x0F) << 12)
                             | (static_cast<std::uint32_t>(*c1) << 6)
                             | *c2;
            if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return std::nullopt;
            return cp;
        }

        if ((first & 0xF8) == 0xF0) {
            advance();
            auto c1 = read_cont();
            auto c2 = read_cont();
            auto c3 = read_cont();
            if (!c1 || !c2 || !c3) return std::nullopt;
            std::uint32_t cp = (static_cast<std::uint32_t>(first & 0x07) << 18)
                             | (static_cast<std::uint32_t>(*c1) << 12)
                             | (static_cast<std::uint32_t>(*c2) << 6)
                             | *c3;
            if (cp < 0x10000 || cp > 0x10FFFF) return std::nullopt;
            return cp;
        }

        return std::nullopt;
    }

    Token lex_ident() {
        SourceLoc start = loc_;
        std::size_t start_pos = pos_;
        
        while (is_alnum_char(peek()) || peek() == '_') {
            advance();
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        
        // hex"55 57 56..." syntax
        if (text == "hex" && peek() == '\"') {
            return lex_hex_string(start, start_pos);
        }
        
        TokenKind kind = keyword_or_ident(text);
        
        Token tok = make_token(kind, text, start);
        return tok;
    }

    TokenKind keyword_or_ident(std::string_view text) {
        // C-style keywords (v2.0 - C++/JS hybrid)
        static const std::unordered_map<std::string_view, TokenKind> c_keywords = {
            // Function/class keywords - ALL ALIASES WORK
            {"function", TokenKind::Function},
            {"func", TokenKind::Function},      // Alias: Go style
            {"fn", TokenKind::Fn},          // Alias: Rust style
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
            return make_token(k, source_.substr(pos_ - 1, 1), start);
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
                    return make_token(TokenKind::PlusEq, "+=", start);
                }
                if (peek() == '+') { 
                    advance(); 
                    return make_token(TokenKind::PlusPlus, "++", start);
                }
                return single(TokenKind::Plus);
            }

            case '-': {
                if (peek() == '>') { 
                    advance(); 
                    return make_token(TokenKind::Arrow, "->", start);
                }
                if (peek() == '=') { 
                    advance(); 
                    return make_token(TokenKind::MinusEq, "-=", start);
                }
                if (peek() == '-') { 
                    advance(); 
                    return make_token(TokenKind::MinusMinus, "--", start);
                }
                return single(TokenKind::Minus);
            }

            case '*': {
                if (peek() == '=') { 
                    advance(); 
                    return make_token(TokenKind::StarEq, "*=", start);
                }
                return single(TokenKind::Star);
            }

            case '/': {
                if (peek() == '=') { 
                    advance(); 
                    return make_token(TokenKind::SlashEq, "/=", start);
                }
                return single(TokenKind::Slash);
            }

            case '%': {
                if (peek() == '=') {
                    advance();
                    return make_token(TokenKind::PercentEq, "%=", start);
                }
                return single(TokenKind::Percent);
            }

            case '&': {
                if (peek() == '&') { 
                    advance(); 
                    return make_token(TokenKind::AndAnd, "&&", start);
                }
                return single(TokenKind::Ampersand);
            }

            case '|': {
                if (peek() == '|') { 
                    advance(); 
                    return make_token(TokenKind::OrOr, "||", start);
                }
                return single(TokenKind::Pipe);
            }

            case '!': {
                if (peek() == '=') { 
                    advance(); 
                    return make_token(TokenKind::Ne, "!=", start);
                }
                return single(TokenKind::Bang);
            }

            case '=': {
                if (peek() == '=') { 
                    advance(); 
                    return make_token(TokenKind::Eq, "==", start);
                }
                if (peek() == '>') { 
                    advance(); 
                    return make_token(TokenKind::FatArrow, "=>", start);
                }
                return single(TokenKind::Assign);
            }

            case '<': {
                if (peek() == '<') { 
                    advance(); 
                    return make_token(TokenKind::Shl, "<<", start);
                }
                if (peek() == '=') { 
                    advance(); 
                    return make_token(TokenKind::Le, "<=", start);
                }
                return single(TokenKind::Lt);
            }

            case '>': {
                if (peek() == '>') { 
                    advance(); 
                    return make_token(TokenKind::Shr, ">>", start);
                }
                if (peek() == '=') { 
                    advance(); 
                    return make_token(TokenKind::Ge, ">=", start);
                }
                return single(TokenKind::Gt);
            }

            case ':': {
                if (peek() == ':') { 
                    advance(); 
                    return make_token(TokenKind::ColonColon, "::", start);
                }
                return single(TokenKind::Colon);
            }

            default: {
                return make_token(TokenKind::Error, source_.substr(pos_ - 1, 1), start);
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
