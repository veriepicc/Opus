// Opus Programming Language
// Abstract Syntax Tree Module

export module opus.ast;

import opus.types;
import std;

export namespace opus::ast {

struct Expr;
struct Stmt;
struct Decl;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;

// ============================================================================
// EXPRESSIONS
// ============================================================================

struct LiteralExpr {
    std::variant<
        std::int64_t,   // Integer literal
        std::uint64_t,  // Unsigned literal
        double,         // Float literal
        bool,           // Bool literal
        std::string,    // String literal
        char32_t        // Char literal
    > value;
    Type type;
};

struct IdentExpr {
    std::string name;
};

struct BinaryExpr {
    enum class Op {
        Add, Sub, Mul, Div, Mod,
        BitAnd, BitOr, BitXor,
        Shl, Shr,
        Eq, Ne, Lt, Gt, Le, Ge,
        And, Or,
        Assign,
        AddAssign, SubAssign, MulAssign, DivAssign,
    };
    Op op;
    ExprPtr lhs;
    ExprPtr rhs;
};

struct UnaryExpr {
    enum class Op {
        Neg,        // -x
        Not,        // !x
        BitNot,     // ~x
        Deref,      // *x
        AddrOf,     // &x
        AddrOfMut,  // &mut x
        PreInc,     // ++x
        PreDec,     // --x
        PostInc,    // x++
        PostDec,    // x--
    };
    Op op;
    ExprPtr operand;
};

struct CallExpr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

struct IndexExpr {
    ExprPtr base;
    ExprPtr index;
};

struct FieldExpr {
    ExprPtr base;
    std::string field;
};

struct CastExpr {
    ExprPtr expr;
    Type target_type;
};

struct ArrayExpr {
    std::vector<ExprPtr> elements;
};

struct StructExpr {
    std::string name;
    std::vector<std::pair<std::string, ExprPtr>> fields;
};

struct IfExpr {
    ExprPtr condition;
    std::vector<StmtPtr> then_block;
    std::optional<std::vector<StmtPtr>> else_block;
};

struct BlockExpr {
    std::vector<StmtPtr> stmts;
    std::optional<ExprPtr> result;  // Last expression (implicit return)
};

// spawn function_name(args) - fires off a new thread, returns a handle
struct SpawnExpr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

// await handle - blocks until the thread finishes, gives back the result
struct AwaitExpr {
    ExprPtr handle;
};

// atomic ops - lock-free shared memory access
struct AtomicOpExpr {
    enum class Op { Add, CAS, Load, Store };
    Op op;
    ExprPtr ptr;
    std::vector<ExprPtr> args;  // val for add/store, expected+desired for cas, empty for load
};

struct Expr {
    std::variant<
        LiteralExpr,
        IdentExpr,
        BinaryExpr,
        UnaryExpr,
        CallExpr,
        IndexExpr,
        FieldExpr,
        CastExpr,
        ArrayExpr,
        StructExpr,
        IfExpr,
        BlockExpr,
        SpawnExpr,
        AwaitExpr,
        AtomicOpExpr
    > kind;
    
    SourceSpan span;
    std::optional<Type> resolved_type;  // Filled during type checking

    template<typename T>
    bool is() const { return std::holds_alternative<T>(kind); }

    template<typename T>
    T& as() { return std::get<T>(kind); }

    template<typename T>
    const T& as() const { return std::get<T>(kind); }
};

// ============================================================================
// STATEMENTS
// ============================================================================

struct LetStmt {
    std::string name;
    std::optional<Type> type;  // Can be inferred
    std::optional<ExprPtr> init;
    bool is_mut = false;
};

struct ExprStmt {
    ExprPtr expr;
};

struct ReturnStmt {
    std::optional<ExprPtr> value;
};

struct IfStmt {
    ExprPtr condition;
    std::vector<StmtPtr> then_block;
    std::optional<std::vector<StmtPtr>> else_block;
};

struct WhileStmt {
    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct ForStmt {
    std::string var_name;
    ExprPtr iterable;
    std::vector<StmtPtr> body;
};

struct LoopStmt {
    std::vector<StmtPtr> body;
};

struct BreakStmt {};
struct ContinueStmt {};

struct BlockStmt {
    std::vector<StmtPtr> stmts;
};

// parallel for i in range(start, end) { body } - splits work across cpu cores
struct ParallelForStmt {
    std::string var_name;
    ExprPtr start;
    ExprPtr end;
    std::vector<StmtPtr> body;
};

struct Stmt {
    std::variant<
        LetStmt,
        ExprStmt,
        ReturnStmt,
        IfStmt,
        WhileStmt,
        ForStmt,
        LoopStmt,
        BreakStmt,
        ContinueStmt,
        BlockStmt,
        ParallelForStmt
    > kind;

    SourceSpan span;

    template<typename T>
    bool is() const { return std::holds_alternative<T>(kind); }

    template<typename T>
    T& as() { return std::get<T>(kind); }

    template<typename T>
    const T& as() const { return std::get<T>(kind); }
};

// ============================================================================
// DECLARATIONS
// ============================================================================

struct Param {
    std::string name;
    Type type;
    bool is_mut = false;
};

struct FnDecl {
    std::string name;
    std::vector<Param> params;
    Type return_type;
    std::vector<StmtPtr> body;
    bool is_extern = false;
    bool is_export = false;
    std::optional<std::string> link_name;  // For FFI
};

struct StructDecl {
    std::string name;
    std::vector<std::pair<std::string, Type>> fields;
    bool is_export = false;
};

// Class = Struct + Methods
struct MethodDecl {
    std::string name;
    std::vector<Param> params;  // First param can be 'self'
    Type return_type;
    std::vector<StmtPtr> body;
    bool is_static = false;
};

struct ClassDecl {
    std::string name;
    std::vector<std::pair<std::string, Type>> fields;
    std::vector<MethodDecl> methods;
    std::optional<std::string> parent;  // For future inheritance
    bool is_export = false;
};

struct EnumDecl {
    std::string name;
    std::vector<std::pair<std::string, std::optional<std::int64_t>>> variants;
    bool is_export = false;
};

struct ConstDecl {
    std::string name;
    Type type;
    ExprPtr value;
    bool is_export = false;
};

struct StaticDecl {
    std::string name;
    Type type;
    std::optional<ExprPtr> init;
    bool is_mut = false;
    bool is_export = false;
};

struct ImportDecl {
    std::string path;  // "std.io" or "clib.stdio"
    std::optional<std::string> alias;
};

// Project configuration (opus.project)
struct ProjectDecl {
    std::string name;
    std::string entry;
    std::string output;
    std::string mode;
    std::vector<std::string> includes;
    bool debug = false;
    std::string healing;  // "auto", "freeze", "off", or empty (resolved from debug flag)
};

struct Decl {
    std::variant<
        FnDecl,
        StructDecl,
        ClassDecl,
        EnumDecl,
        ConstDecl,
        StaticDecl,
        ImportDecl,
        ProjectDecl
    > kind;

    SourceSpan span;

    template<typename T>
    bool is() const { return std::holds_alternative<T>(kind); }

    template<typename T>
    T& as() { return std::get<T>(kind); }

    template<typename T>
    const T& as() const { return std::get<T>(kind); }
};

// ============================================================================
// PROGRAM (Top-level)
// ============================================================================

struct Module {
    std::string name;
    std::vector<DeclPtr> decls;
    
    SyntaxMode syntax = SyntaxMode::CStyle;
    std::string source_file;
};

// ============================================================================
// AST UTILITIES
// ============================================================================

inline ExprPtr make_literal_i64(std::int64_t val, SourceSpan span = {}) {
    auto expr = std::make_unique<Expr>();
    LiteralExpr lit;
    lit.value = val;
    lit.type = Type::make_primitive(PrimitiveType::I64);
    expr->kind.emplace<LiteralExpr>(std::move(lit));
    expr->span = span;
    return expr;
}

inline ExprPtr make_literal_f64(double val, SourceSpan span = {}) {
    auto expr = std::make_unique<Expr>();
    LiteralExpr lit;
    lit.value = val;
    lit.type = Type::make_primitive(PrimitiveType::F64);
    expr->kind.emplace<LiteralExpr>(std::move(lit));
    expr->span = span;
    return expr;
}

inline ExprPtr make_literal_bool(bool val, SourceSpan span = {}) {
    auto expr = std::make_unique<Expr>();
    LiteralExpr lit;
    lit.value = val;
    lit.type = Type::make_primitive(PrimitiveType::Bool);
    expr->kind.emplace<LiteralExpr>(std::move(lit));
    expr->span = span;
    return expr;
}

inline ExprPtr make_literal_str(std::string val, SourceSpan span = {}) {
    auto expr = std::make_unique<Expr>();
    LiteralExpr lit;
    lit.value = std::move(val);
    lit.type = Type::make_primitive(PrimitiveType::Str);
    expr->kind.emplace<LiteralExpr>(std::move(lit));
    expr->span = span;
    return expr;
}

inline ExprPtr make_ident(std::string name, SourceSpan span = {}) {
    auto expr = std::make_unique<Expr>();
    IdentExpr ident;
    ident.name = std::move(name);
    expr->kind.emplace<IdentExpr>(std::move(ident));
    expr->span = span;
    return expr;
}

inline ExprPtr make_binary(BinaryExpr::Op op, ExprPtr lhs, ExprPtr rhs, SourceSpan span = {}) {
    auto expr = std::make_unique<Expr>();
    BinaryExpr bin;
    bin.op = op;
    bin.lhs = std::move(lhs);
    bin.rhs = std::move(rhs);
    expr->kind.emplace<BinaryExpr>(std::move(bin));
    expr->span = span;
    return expr;
}

inline ExprPtr make_call(ExprPtr callee, std::vector<ExprPtr> args, SourceSpan span = {}) {
    auto expr = std::make_unique<Expr>();
    CallExpr call;
    call.callee = std::move(callee);
    call.args = std::move(args);
    expr->kind.emplace<CallExpr>(std::move(call));
    expr->span = span;
    return expr;
}

inline StmtPtr make_let(std::string name, std::optional<Type> type, 
                         ExprPtr init, bool is_mut = false, SourceSpan span = {}) {
    auto stmt = std::make_unique<Stmt>();
    LetStmt let;
    let.name = std::move(name);
    let.type = std::move(type);
    if (init) {
        let.init = std::make_optional(std::move(init));
    }
    let.is_mut = is_mut;
    stmt->kind.emplace<LetStmt>(std::move(let));
    stmt->span = span;
    return stmt;
}

inline StmtPtr make_return(ExprPtr value = nullptr, SourceSpan span = {}) {
    auto stmt = std::make_unique<Stmt>();
    ReturnStmt ret;
    ret.value = value ? std::make_optional(std::move(value)) : std::nullopt;
    stmt->kind.emplace<ReturnStmt>(std::move(ret));
    stmt->span = span;
    return stmt;
}

inline StmtPtr make_expr_stmt(ExprPtr expr, SourceSpan span = {}) {
    auto stmt = std::make_unique<Stmt>();
    ExprStmt es;
    es.expr = std::move(expr);
    stmt->kind.emplace<ExprStmt>(std::move(es));
    stmt->span = span;
    return stmt;
}

} // namespace opus::ast

