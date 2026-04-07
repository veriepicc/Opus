// Opus Programming Language
// Abstract Syntax Tree Module

export module opus.ast;

import opus.types;
import std;

export namespace opus::ast {

struct Expr;
struct Stmt;
struct Decl;
struct Param;
struct MethodDecl;

template<typename T>
using NodePtr = std::unique_ptr<T>;

template<typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using ExprPtr = NodePtr<Expr>;
using StmtPtr = NodePtr<Stmt>;
using DeclPtr = NodePtr<Decl>;

using ExprList = std::vector<ExprPtr>;
using StmtList = std::vector<StmtPtr>;
using DeclList = std::vector<DeclPtr>;
using StmtBlock = StmtList;

// stmt suites are the shared shape for owned statement sequences
// they let bodies stay explicit without leaking raw vector semantics everywhere
struct StmtSuite {
    StmtBlock stmts;

    using iterator = StmtBlock::iterator;
    using const_iterator = StmtBlock::const_iterator;
    using size_type = StmtBlock::size_type;
    using value_type = StmtBlock::value_type;

    StmtSuite() = default;
    StmtSuite(StmtBlock block)
        : stmts(std::move(block)) {}

    [[nodiscard]] bool empty() const noexcept { return stmts.empty(); }
    [[nodiscard]] size_type size() const noexcept { return stmts.size(); }

    [[nodiscard]] iterator begin() noexcept { return stmts.begin(); }
    [[nodiscard]] const_iterator begin() const noexcept { return stmts.begin(); }
    [[nodiscard]] iterator end() noexcept { return stmts.end(); }
    [[nodiscard]] const_iterator end() const noexcept { return stmts.end(); }

    [[nodiscard]] value_type& front() noexcept { return stmts.front(); }
    [[nodiscard]] const value_type& front() const noexcept { return stmts.front(); }
    [[nodiscard]] value_type& back() noexcept { return stmts.back(); }
    [[nodiscard]] const value_type& back() const noexcept { return stmts.back(); }

    [[nodiscard]] value_type& operator[](size_type index) noexcept { return stmts[index]; }
    [[nodiscard]] const value_type& operator[](size_type index) const noexcept { return stmts[index]; }

    operator StmtBlock&() noexcept { return stmts; }
    operator const StmtBlock&() const noexcept { return stmts; }
};

struct NodeId {
    std::uint32_t value = 0;

    [[nodiscard]] bool is_valid() const noexcept { return value != 0; }
    auto operator<=>(const NodeId&) const = default;
};

[[nodiscard]] inline NodeId next_node_id() {
    static std::atomic<std::uint32_t> next{1};
    return NodeId{next.fetch_add(1, std::memory_order_relaxed)};
}

struct Block {
    StmtSuite stmts;
    ExprPtr result;

    [[nodiscard]] bool has_result() const noexcept { return static_cast<bool>(result); }
    [[nodiscard]] bool empty() const noexcept { return stmts.empty() && !result; }
};

struct FieldInit {
    std::string name;
    ExprPtr value;
};

using FieldInitList = std::vector<FieldInit>;

struct FieldDecl {
    std::string name;
    Type type;
};

using FieldDeclList = std::vector<FieldDecl>;

struct EnumVariantDecl {
    std::string name;
    std::optional<std::int64_t> value;
};

using EnumVariantList = std::vector<EnumVariantDecl>;

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
        AddAssign, SubAssign, MulAssign, DivAssign, ModAssign,
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
    ExprList args;
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
    ExprList elements;
};

struct StructExpr {
    std::string name;
    FieldInitList fields;
};

struct IfExpr {
    ExprPtr condition;
    StmtSuite then_block;
    std::optional<StmtSuite> else_block;

    [[nodiscard]] bool has_else() const noexcept { return else_block.has_value(); }
};

struct BlockExpr {
    Block block;  // Last expression lives in block.result

    [[nodiscard]] bool has_result() const noexcept { return block.has_result(); }
};

// spawn function_name(args) - fires off a new thread, returns a handle
struct SpawnExpr {
    ExprPtr callee;
    ExprList args;
};

// await handle - blocks until the thread finishes, gives back the result
struct AwaitExpr {
    ExprPtr handle;
};

// atomic ops - lock-free shared memory access
struct AtomicLoadExpr {
    ExprPtr ptr;
};

struct AtomicStoreExpr {
    ExprPtr ptr;
    ExprPtr value;
};

struct AtomicAddExpr {
    ExprPtr ptr;
    ExprPtr value;
};

struct AtomicCompareExchangeExpr {
    ExprPtr ptr;
    ExprPtr expected;
    ExprPtr desired;
};

struct Expr {
    using Kind = std::variant<
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
        AtomicLoadExpr,
        AtomicStoreExpr,
        AtomicAddExpr,
        AtomicCompareExchangeExpr
    >;

    NodeId id = next_node_id();
    Kind kind;
    SourceSpan span;
    std::optional<Type> resolved_type;

    template<typename T>
    [[nodiscard]] bool is() const { return std::holds_alternative<T>(kind); }

    template<typename T>
    [[nodiscard]] T& as() {
        if (auto* p = std::get_if<T>(&kind)) return *p;
        throw std::logic_error(std::format(
            "expected ExprKind to hold {} but got index {} at line {}",
            typeid(T).name(), kind.index(), span.start.line));
    }

    template<typename T>
    [[nodiscard]] const T& as() const {
        if (auto* p = std::get_if<T>(&kind)) return *p;
        throw std::logic_error(std::format(
            "expected ExprKind to hold {} but got index {} at line {}",
            typeid(T).name(), kind.index(), span.start.line));
    }

    template<typename Visitor>
    decltype(auto) visit(Visitor&& visitor) {
        return std::visit(std::forward<Visitor>(visitor), kind);
    }

    template<typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        return std::visit(std::forward<Visitor>(visitor), kind);
    }

    [[nodiscard]] bool has_resolved_type() const noexcept { return resolved_type.has_value(); }
    [[nodiscard]] const Type* resolved_type_ptr() const noexcept { return resolved_type ? &*resolved_type : nullptr; }
    [[nodiscard]] NodeId node_id() const noexcept { return id; }
};

// ============================================================================
// STATEMENTS
// ============================================================================

struct LetStmt {
    std::string name;
    std::optional<Type> type;  // Can be inferred
    ExprPtr init;
    bool is_mut = false;

    [[nodiscard]] bool has_init() const noexcept { return static_cast<bool>(init); }
};

struct ExprStmt {
    ExprPtr expr;
};

struct ReturnStmt {
    ExprPtr value;

    [[nodiscard]] bool has_value() const noexcept { return static_cast<bool>(value); }
};

struct IfStmt {
    ExprPtr condition;
    StmtSuite then_block;
    std::optional<StmtSuite> else_block;

    [[nodiscard]] bool has_else() const noexcept { return else_block.has_value(); }
};

struct WhileStmt {
    ExprPtr condition;
    StmtSuite body;
};

struct ForStmt {
    std::string name;
    ExprPtr iterable;
    StmtSuite body;
};

struct LoopStmt {
    StmtSuite body;
};

struct BreakStmt {};
struct ContinueStmt {};

struct BlockStmt {
    Block block;
};

// parallel for i in range(start, end) { body } - splits work across cpu cores
struct ParallelForStmt {
    std::string name;
    ExprPtr start;
    ExprPtr end;
    StmtSuite body;
};

struct Stmt {
    using Kind = std::variant<
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
    >;

    NodeId id = next_node_id();
    Kind kind;
    SourceSpan span;

    template<typename T>
    [[nodiscard]] bool is() const { return std::holds_alternative<T>(kind); }

    template<typename T>
    [[nodiscard]] T& as() {
        if (auto* p = std::get_if<T>(&kind)) return *p;
        throw std::logic_error(std::format(
            "expected StmtKind to hold {} but got index {} at line {}",
            typeid(T).name(), kind.index(), span.start.line));
    }

    template<typename T>
    [[nodiscard]] const T& as() const {
        if (auto* p = std::get_if<T>(&kind)) return *p;
        throw std::logic_error(std::format(
            "expected StmtKind to hold {} but got index {} at line {}",
            typeid(T).name(), kind.index(), span.start.line));
    }

    template<typename Visitor>
    decltype(auto) visit(Visitor&& visitor) {
        return std::visit(std::forward<Visitor>(visitor), kind);
    }

    template<typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        return std::visit(std::forward<Visitor>(visitor), kind);
    }

    [[nodiscard]] NodeId node_id() const noexcept { return id; }
};

// ============================================================================
// DECLARATIONS
// ============================================================================

struct Param {
    std::string name;
    Type type;
    bool is_mut = false;
};

using ParamList = std::vector<Param>;

enum class Linkage {
    Internal,
    External,
};

struct DeclAttrs {
    bool is_export = false;
    Linkage linkage = Linkage::Internal;
    std::optional<std::string> link_name;

    [[nodiscard]] bool is_extern() const noexcept { return linkage == Linkage::External; }
    [[nodiscard]] bool has_link_name() const noexcept { return link_name.has_value(); }
};

struct FnDecl {
    std::string name;
    ParamList params;
    Type return_type;
    std::optional<StmtSuite> body;
    DeclAttrs attrs;

    [[nodiscard]] bool has_body() const noexcept { return body.has_value(); }
    [[nodiscard]] bool is_declaration_only() const noexcept { return attrs.is_extern() && !body.has_value(); }
    [[nodiscard]] StmtSuite& body_ref() { return *body; }
    [[nodiscard]] const StmtSuite& body_ref() const { return *body; }
};

struct StructDecl {
    std::string name;
    FieldDeclList fields;
    DeclAttrs attrs;
};

enum class MethodReceiver {
    ImplicitSelf,
    Static,
};

// Class = Struct + Methods
struct MethodDecl {
    std::string name;
    ParamList params;
    Type return_type;
    StmtSuite body;
    MethodReceiver receiver = MethodReceiver::ImplicitSelf;

    [[nodiscard]] bool has_receiver() const noexcept { return receiver == MethodReceiver::ImplicitSelf; }
};

using MethodList = std::vector<MethodDecl>;

struct ClassDecl {
    std::string name;
    FieldDeclList fields;
    MethodList methods;
    DeclAttrs attrs;
};

struct EnumDecl {
    std::string name;
    EnumVariantList variants;
    DeclAttrs attrs;
};

struct ConstDecl {
    std::string name;
    Type type;
    ExprPtr value;
    DeclAttrs attrs;
};

struct StaticDecl {
    std::string name;
    Type type;
    ExprPtr init;
    bool is_mut = false;
    DeclAttrs attrs;

    [[nodiscard]] bool has_init() const noexcept { return static_cast<bool>(init); }
};

struct ImportDecl {
    std::string path;  // "std.io" or "clib.stdio"
    std::optional<std::string> alias;
};

struct TypeAliasDecl {
    std::string name;
    Type target;
};

// healing mode for crash recovery
enum class HealingMode { Auto, Freeze, Off };

// Project configuration (opus.project)
struct ProjectDecl {
    std::string name;
    std::string entry;
    std::string output;
    std::string mode;
    std::vector<std::string> includes;
    bool debug = false;
    std::optional<HealingMode> healing;
};

struct Decl {
    using Kind = std::variant<
        FnDecl,
        StructDecl,
        ClassDecl,
        EnumDecl,
        ConstDecl,
        StaticDecl,
        ImportDecl,
        TypeAliasDecl
    >;

    NodeId id = next_node_id();
    Kind kind;
    SourceSpan span;

    template<typename T>
    [[nodiscard]] bool is() const { return std::holds_alternative<T>(kind); }

    template<typename T>
    [[nodiscard]] T& as() {
        if (auto* p = std::get_if<T>(&kind)) return *p;
        throw std::logic_error(std::format(
            "expected DeclKind to hold {} but got index {} at line {}",
            typeid(T).name(), kind.index(), span.start.line));
    }

    template<typename T>
    [[nodiscard]] const T& as() const {
        if (auto* p = std::get_if<T>(&kind)) return *p;
        throw std::logic_error(std::format(
            "expected DeclKind to hold {} but got index {} at line {}",
            typeid(T).name(), kind.index(), span.start.line));
    }

    template<typename Visitor>
    decltype(auto) visit(Visitor&& visitor) {
        return std::visit(std::forward<Visitor>(visitor), kind);
    }

    template<typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        return std::visit(std::forward<Visitor>(visitor), kind);
    }

    [[nodiscard]] NodeId node_id() const noexcept { return id; }
};

// ============================================================================
// PROGRAM (Top-level)
// ============================================================================

struct Module {
    std::string name;
    DeclList decls;

    SyntaxMode syntax = SyntaxMode::CStyle;
    std::string source_file;
};

struct AstValidationError {
    std::string message;
    SourceLoc loc;
    NodeId node_id;

    [[nodiscard]] std::string to_string() const {
        if (node_id.is_valid()) {
            return std::format("{}: ast error [{}]: {}", loc.to_string(), node_id.value, message);
        }
        return std::format("{}: ast error: {}", loc.to_string(), message);
    }
};

// ============================================================================
// AST UTILITIES
// ============================================================================

template<typename Node, typename Payload>
[[nodiscard]] inline NodePtr<Node> make_node(Payload&& payload, SourceSpan span = {}) {
    auto node = std::make_unique<Node>();
    node->kind = std::forward<Payload>(payload);
    node->span = span;
    return node;
}

template<typename T>
[[nodiscard]] inline ExprPtr make_expr(T&& node, SourceSpan span = {}) {
    return make_node<Expr>(std::forward<T>(node), span);
}

template<typename T>
[[nodiscard]] inline StmtPtr make_stmt(T&& node, SourceSpan span = {}) {
    return make_node<Stmt>(std::forward<T>(node), span);
}

template<typename T>
[[nodiscard]] inline DeclPtr make_decl(T&& node, SourceSpan span = {}) {
    return make_node<Decl>(std::forward<T>(node), span);
}

[[nodiscard]] inline ExprPtr make_literal_i64(std::int64_t val, SourceSpan span = {}) {
    return make_expr(LiteralExpr{ .value = val, .type = Type::make_primitive(PrimitiveType::I64) }, span);
}

[[nodiscard]] inline ExprPtr make_literal_f64(double val, SourceSpan span = {}) {
    return make_expr(LiteralExpr{ .value = val, .type = Type::make_primitive(PrimitiveType::F64) }, span);
}

[[nodiscard]] inline ExprPtr make_literal_bool(bool val, SourceSpan span = {}) {
    return make_expr(LiteralExpr{ .value = val, .type = Type::make_primitive(PrimitiveType::Bool) }, span);
}

[[nodiscard]] inline ExprPtr make_literal_str(std::string val, SourceSpan span = {}) {
    LiteralExpr lit;
    lit.value = std::move(val);
    lit.type = Type::make_primitive(PrimitiveType::Str);
    return make_expr(std::move(lit), span);
}

[[nodiscard]] inline ExprPtr make_ident(std::string name, SourceSpan span = {}) {
    IdentExpr ident;
    ident.name = std::move(name);
    return make_expr(std::move(ident), span);
}

[[nodiscard]] inline ExprPtr make_binary(BinaryExpr::Op op, ExprPtr lhs, ExprPtr rhs, SourceSpan span = {}) {
    BinaryExpr bin;
    bin.op = op;
    bin.lhs = std::move(lhs);
    bin.rhs = std::move(rhs);
    return make_expr(std::move(bin), span);
}

[[nodiscard]] inline ExprPtr make_call(ExprPtr callee, ExprList args, SourceSpan span = {}) {
    CallExpr call;
    call.callee = std::move(callee);
    call.args = std::move(args);
    return make_expr(std::move(call), span);
}

[[nodiscard]] inline StmtPtr make_let(
    std::string name,
    std::optional<Type> type,
    ExprPtr init = nullptr,
    bool is_mut = false,
    SourceSpan span = {}) {
    LetStmt let;
    let.name = std::move(name);
    let.type = std::move(type);
    let.init = std::move(init);
    let.is_mut = is_mut;
    return make_stmt(std::move(let), span);
}

[[nodiscard]] inline StmtPtr make_return(ExprPtr value = nullptr, SourceSpan span = {}) {
    ReturnStmt ret;
    ret.value = std::move(value);
    return make_stmt(std::move(ret), span);
}

[[nodiscard]] inline StmtPtr make_expr_stmt(ExprPtr expr, SourceSpan span = {}) {
    ExprStmt es;
    es.expr = std::move(expr);
    return make_stmt(std::move(es), span);
}

template<typename ExprT, typename Visitor>
inline void walk_expr(ExprT& expr, Visitor&& visitor);

template<typename StmtT, typename Visitor>
inline void walk_stmt(StmtT& stmt, Visitor&& visitor);

template<typename DeclNodeT, typename Visitor>
inline void walk_decl_ptr(NodePtr<DeclNodeT>& decl, Visitor&& visitor);

template<typename DeclNodeT, typename Visitor>
inline void walk_decl_ptr(const NodePtr<DeclNodeT>& decl, Visitor&& visitor);

template<typename ExprNodeT, typename Visitor>
inline void walk_expr_ptr(NodePtr<ExprNodeT>& expr, Visitor&& visitor) {
    if (expr) {
        walk_expr(*expr, std::forward<Visitor>(visitor));
    }
}

template<typename ExprNodeT, typename Visitor>
inline void walk_expr_ptr(const NodePtr<ExprNodeT>& expr, Visitor&& visitor) {
    if (expr) {
        walk_expr(std::as_const(*expr), std::forward<Visitor>(visitor));
    }
}

template<typename StmtNodeT, typename Visitor>
inline void walk_stmt_ptr(NodePtr<StmtNodeT>& stmt, Visitor&& visitor) {
    if (stmt) {
        walk_stmt(*stmt, std::forward<Visitor>(visitor));
    }
}

template<typename StmtNodeT, typename Visitor>
inline void walk_stmt_ptr(const NodePtr<StmtNodeT>& stmt, Visitor&& visitor) {
    if (stmt) {
        walk_stmt(std::as_const(*stmt), std::forward<Visitor>(visitor));
    }
}

template<typename BlockT, typename Visitor>
inline void walk_stmt_block(BlockT& block, Visitor&& visitor) {
    for (auto& stmt : block) {
        walk_stmt_ptr(stmt, std::forward<Visitor>(visitor));
    }
}

template<typename BlockT, typename Visitor>
inline void walk_block(BlockT& block, Visitor&& visitor) {
    walk_stmt_block(block.stmts, std::forward<Visitor>(visitor));
    walk_expr_ptr(block.result, std::forward<Visitor>(visitor));
}

template<typename ExprT, typename Visitor>
inline void walk_expr(ExprT& expr, Visitor&& visitor) {
    auto&& visit = visitor;
    visit(expr);

    expr.visit(overloaded{
        [&](auto& e) requires (
            std::same_as<std::remove_cvref_t<decltype(e)>, LiteralExpr> ||
            std::same_as<std::remove_cvref_t<decltype(e)>, IdentExpr>) {},
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, BinaryExpr> {
            walk_expr_ptr(e.lhs, visit);
            walk_expr_ptr(e.rhs, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, UnaryExpr> {
            walk_expr_ptr(e.operand, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, CallExpr> {
            walk_expr_ptr(e.callee, visit);
            for (auto& arg : e.args) {
                walk_expr_ptr(arg, visit);
            }
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, IndexExpr> {
            walk_expr_ptr(e.base, visit);
            walk_expr_ptr(e.index, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, FieldExpr> {
            walk_expr_ptr(e.base, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, CastExpr> {
            walk_expr_ptr(e.expr, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, ArrayExpr> {
            for (auto& elem : e.elements) {
                walk_expr_ptr(elem, visit);
            }
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, StructExpr> {
            for (auto& field : e.fields) {
                walk_expr_ptr(field.value, visit);
            }
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, IfExpr> {
            walk_expr_ptr(e.condition, visit);
            walk_stmt_block(e.then_block, visit);
            if (e.else_block) {
                walk_stmt_block(*e.else_block, visit);
            }
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, BlockExpr> {
            walk_block(e.block, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, SpawnExpr> {
            walk_expr_ptr(e.callee, visit);
            for (auto& arg : e.args) {
                walk_expr_ptr(arg, visit);
            }
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, AwaitExpr> {
            walk_expr_ptr(e.handle, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, AtomicLoadExpr> {
            walk_expr_ptr(e.ptr, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, AtomicStoreExpr> {
            walk_expr_ptr(e.ptr, visit);
            walk_expr_ptr(e.value, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, AtomicAddExpr> {
            walk_expr_ptr(e.ptr, visit);
            walk_expr_ptr(e.value, visit);
        },
        [&](auto& e) requires std::same_as<std::remove_cvref_t<decltype(e)>, AtomicCompareExchangeExpr> {
            walk_expr_ptr(e.ptr, visit);
            walk_expr_ptr(e.expected, visit);
            walk_expr_ptr(e.desired, visit);
        },
    });
}

template<typename StmtT, typename Visitor>
inline void walk_stmt(StmtT& stmt, Visitor&& visitor) {
    auto&& visit = visitor;
    visit(stmt);

    stmt.visit(overloaded{
        [&](auto& s) requires (
            std::same_as<std::remove_cvref_t<decltype(s)>, BreakStmt> ||
            std::same_as<std::remove_cvref_t<decltype(s)>, ContinueStmt>) {},
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, LetStmt> {
            walk_expr_ptr(s.init, visit);
        },
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, ExprStmt> {
            walk_expr_ptr(s.expr, visit);
        },
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, ReturnStmt> {
            walk_expr_ptr(s.value, visit);
        },
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, IfStmt> {
            walk_expr_ptr(s.condition, visit);
            walk_stmt_block(s.then_block, visit);
            if (s.else_block) {
                walk_stmt_block(*s.else_block, visit);
            }
        },
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, WhileStmt> {
            walk_expr_ptr(s.condition, visit);
            walk_stmt_block(s.body, visit);
        },
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, ForStmt> {
            walk_expr_ptr(s.iterable, visit);
            walk_stmt_block(s.body, visit);
        },
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, LoopStmt> {
            walk_stmt_block(s.body, visit);
        },
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, BlockStmt> {
            walk_block(s.block, visit);
        },
        [&](auto& s) requires std::same_as<std::remove_cvref_t<decltype(s)>, ParallelForStmt> {
            walk_expr_ptr(s.start, visit);
            walk_expr_ptr(s.end, visit);
            walk_stmt_block(s.body, visit);
        },
    });
}

template<typename DeclT, typename Visitor>
inline void walk_decl(DeclT& decl, Visitor&& visitor) {
    auto&& visit = visitor;
    visit(decl);

    decl.visit(overloaded{
        [&](auto& d) requires (
            std::same_as<std::remove_cvref_t<decltype(d)>, ImportDecl> ||
            std::same_as<std::remove_cvref_t<decltype(d)>, TypeAliasDecl> ||
            std::same_as<std::remove_cvref_t<decltype(d)>, StructDecl> ||
            std::same_as<std::remove_cvref_t<decltype(d)>, EnumDecl>) {},
        [&](auto& d) requires std::same_as<std::remove_cvref_t<decltype(d)>, ConstDecl> {
            walk_expr_ptr(d.value, visit);
        },
        [&](auto& d) requires std::same_as<std::remove_cvref_t<decltype(d)>, StaticDecl> {
            walk_expr_ptr(d.init, visit);
        },
        [&](auto& d) requires std::same_as<std::remove_cvref_t<decltype(d)>, FnDecl> {
            if (d.body) {
                walk_stmt_block(*d.body, visit);
            }
        },
        [&](auto& d) requires std::same_as<std::remove_cvref_t<decltype(d)>, ClassDecl> {
            for (auto& method : d.methods) {
                walk_stmt_block(method.body, visit);
            }
        },
    });
}

template<typename ModuleT, typename Visitor>
inline void walk_module(ModuleT& mod, Visitor&& visitor) {
    auto&& visit = visitor;
    for (auto& decl : mod.decls) {
        walk_decl_ptr(decl, visit);
    }
}

template<typename DeclNodeT, typename Visitor>
inline void walk_decl_ptr(NodePtr<DeclNodeT>& decl, Visitor&& visitor) {
    if (decl) {
        walk_decl(*decl, std::forward<Visitor>(visitor));
    }
}

template<typename DeclNodeT, typename Visitor>
inline void walk_decl_ptr(const NodePtr<DeclNodeT>& decl, Visitor&& visitor) {
    if (decl) {
        walk_decl(std::as_const(*decl), std::forward<Visitor>(visitor));
    }
}

namespace detail {

[[nodiscard]] inline SourceLoc module_loc(const Module& mod) {
    SourceLoc loc;
    if (!mod.source_file.empty()) {
        loc.file = mod.source_file;
    } else if (!mod.name.empty()) {
        loc.file = mod.name;
    }
    return loc;
}

template<typename NodeT>
[[nodiscard]] inline SourceLoc node_loc(const NodeT& node) {
    return node.span.start;
}

template<typename NodeT>
[[nodiscard]] inline NodeId node_id_of(const NodeT& node) {
    return node.node_id();
}

template<typename NodeT>
[[nodiscard]] inline const char* node_kind_name() {
    if constexpr (std::same_as<std::remove_cvref_t<NodeT>, Expr>) {
        return "expression";
    } else if constexpr (std::same_as<std::remove_cvref_t<NodeT>, Stmt>) {
        return "statement";
    } else {
        return "declaration";
    }
}

class AstValidator {
public:
    explicit AstValidator(const Module& module)
        : module_(module) {}

    [[nodiscard]] std::vector<AstValidationError> finish() && {
        return std::move(errors_);
    }

    void validate_module() {
        if (module_.name.empty()) {
            report(module_loc(module_), "module name cannot be empty");
        }

        for (const auto& decl : module_.decls) {
            if (!decl) {
                report(module_loc(module_), "module contains a null top-level declaration");
                continue;
            }
            walk_decl(*decl, [this](const auto& node) {
                using Node = std::remove_cvref_t<decltype(node)>;
                if constexpr (std::same_as<Node, Expr>) {
                    validate_expr(node);
                } else if constexpr (std::same_as<Node, Stmt>) {
                    validate_stmt(node);
                } else if constexpr (std::same_as<Node, Decl>) {
                    validate_decl(node);
                }
            });
        }
    }

private:
    const Module& module_;
    std::vector<AstValidationError> errors_;
    std::unordered_set<std::uint32_t> seen_node_ids_;

    void report(const SourceLoc& loc, std::string message, NodeId node_id = {}) {
        errors_.push_back(AstValidationError{
            .message = std::move(message),
            .loc = loc,
            .node_id = node_id
        });
    }

    template<typename NodeT>
    void report(const NodeT& node, std::string message) {
        report(node_loc(node), std::move(message), node_id_of(node));
    }

    template<typename NodeT>
    void validate_node_header(const NodeT& node) {
        if (!node.node_id().is_valid()) {
            report(node, std::format("{} has an invalid node id", node_kind_name<NodeT>()));
        } else if (!seen_node_ids_.insert(node.node_id().value).second) {
            report(node, std::format("{} reuses node id {}", node_kind_name<NodeT>(), node.node_id().value));
        }
    }

    template<typename NodeT, typename PtrT>
    void require_child(const NodeT& owner, const PtrT& ptr, std::string_view label) {
        if (!ptr) {
            report(owner, std::format("{} is missing {}", node_kind_name<NodeT>(), label));
        }
    }

    template<typename NodeT, typename RangeT>
    void require_no_null_stmt_ptrs(const NodeT& owner, const RangeT& stmts, std::string_view label) {
        for (const auto& stmt : stmts) {
            if (!stmt) {
                report(owner, std::format("{} contains a null statement in {}", node_kind_name<NodeT>(), label));
            }
        }
    }

    template<typename NodeT>
    void validate_name(const NodeT& owner, std::string_view name, std::string_view label) {
        if (name.empty()) {
            report(owner, std::format("{} cannot have an empty {}", node_kind_name<NodeT>(), label));
        }
    }

template<typename NodeT>
    void validate_block(const NodeT& owner, const StmtBlock& block, std::string_view label) {
        require_no_null_stmt_ptrs(owner, block, label);
    }

    template<typename NodeT>
    void validate_block(const NodeT& owner, const StmtSuite& block, std::string_view label) {
        validate_block(owner, block.stmts, label);
    }

    template<typename NodeT>
    void validate_block(const NodeT& owner, const Block& block, std::string_view label) {
        validate_block(owner, block.stmts, label);
    }

    template<typename NodeT>
    void validate_attrs(const NodeT& owner, const DeclAttrs& attrs) {
        if (attrs.has_link_name() && attrs.link_name->empty()) {
            report(owner, "declaration link name cannot be empty");
        }
    }

    template<typename NodeT, typename RangeT, typename NameFn>
    void validate_unique_names(const NodeT& owner, const RangeT& items, NameFn&& name_fn, std::string_view label) {
        std::unordered_set<std::string_view> seen;
        for (const auto& item : items) {
            auto&& name = name_fn(item);
            if (name.empty()) {
                report(owner, std::format("{} contains an empty {}", node_kind_name<NodeT>(), label));
                continue;
            }
            if (!seen.insert(std::string_view{name}).second) {
                report(owner, std::format("{} contains duplicate {} '{}'", node_kind_name<NodeT>(), label, name));
            }
        }
    }

    void validate_params(const auto& owner, const ParamList& params) {
        validate_unique_names(owner, params, [](const Param& param) -> const std::string& {
            return param.name;
        }, "parameter name");
    }

    void validate_expr(const Expr& expr) {
        validate_node_header(expr);

        expr.visit(overloaded{
            [&](const LiteralExpr&) {},
            [&](const IdentExpr& e) {
                validate_name(expr, e.name, "identifier name");
            },
            [&](const BinaryExpr& e) {
                require_child(expr, e.lhs, "binary lhs");
                require_child(expr, e.rhs, "binary rhs");
            },
            [&](const UnaryExpr& e) {
                require_child(expr, e.operand, "unary operand");
            },
            [&](const CallExpr& e) {
                require_child(expr, e.callee, "call callee");
                for (const auto& arg : e.args) {
                    require_child(expr, arg, "call argument");
                }
            },
            [&](const IndexExpr& e) {
                require_child(expr, e.base, "index base");
                require_child(expr, e.index, "index expression");
            },
            [&](const FieldExpr& e) {
                require_child(expr, e.base, "field base");
                validate_name(expr, e.field, "field name");
            },
            [&](const CastExpr& e) {
                require_child(expr, e.expr, "cast operand");
            },
            [&](const ArrayExpr& e) {
                for (const auto& elem : e.elements) {
                    require_child(expr, elem, "array element");
                }
            },
            [&](const StructExpr& e) {
                validate_name(expr, e.name, "struct literal type name");
                validate_unique_names(expr, e.fields, [](const FieldInit& field) -> const std::string& {
                    return field.name;
                }, "field name");
                for (const auto& field : e.fields) {
                    require_child(expr, field.value, "struct field value");
                }
            },
            [&](const IfExpr& e) {
                require_child(expr, e.condition, "if condition");
                validate_block(expr, e.then_block, "if then block");
                if (e.else_block) {
                    validate_block(expr, *e.else_block, "if else block");
                }
            },
            [&](const BlockExpr& e) {
                validate_block(expr, e.block, "block expression");
            },
            [&](const SpawnExpr& e) {
                require_child(expr, e.callee, "spawn callee");
                for (const auto& arg : e.args) {
                    require_child(expr, arg, "spawn argument");
                }
            },
            [&](const AwaitExpr& e) {
                require_child(expr, e.handle, "await handle");
            },
            [&](const AtomicLoadExpr& e) {
                require_child(expr, e.ptr, "atomic pointer");
            },
            [&](const AtomicStoreExpr& e) {
                require_child(expr, e.ptr, "atomic pointer");
                require_child(expr, e.value, "atomic store value");
            },
            [&](const AtomicAddExpr& e) {
                require_child(expr, e.ptr, "atomic pointer");
                require_child(expr, e.value, "atomic add value");
            },
            [&](const AtomicCompareExchangeExpr& e) {
                require_child(expr, e.ptr, "atomic pointer");
                require_child(expr, e.expected, "atomic compare-exchange expected value");
                require_child(expr, e.desired, "atomic compare-exchange desired value");
            },
        });
    }

    void validate_stmt(const Stmt& stmt) {
        validate_node_header(stmt);

        stmt.visit(overloaded{
            [&](const BreakStmt&) {},
            [&](const ContinueStmt&) {},
            [&](const LetStmt& s) {
                validate_name(stmt, s.name, "binding name");
            },
            [&](const ExprStmt& s) {
                require_child(stmt, s.expr, "expression");
            },
            [&](const ReturnStmt&) {},
            [&](const IfStmt& s) {
                require_child(stmt, s.condition, "if condition");
                validate_block(stmt, s.then_block, "if then block");
                if (s.else_block) {
                    validate_block(stmt, *s.else_block, "if else block");
                }
            },
            [&](const WhileStmt& s) {
                require_child(stmt, s.condition, "while condition");
                validate_block(stmt, s.body, "while body");
            },
            [&](const ForStmt& s) {
                validate_name(stmt, s.name, "loop variable");
                require_child(stmt, s.iterable, "for iterable");
                validate_block(stmt, s.body, "for body");
            },
            [&](const LoopStmt& s) {
                validate_block(stmt, s.body, "loop body");
            },
            [&](const BlockStmt& s) {
                validate_block(stmt, s.block, "statement block");
            },
            [&](const ParallelForStmt& s) {
                validate_name(stmt, s.name, "parallel loop variable");
                require_child(stmt, s.start, "parallel range start");
                require_child(stmt, s.end, "parallel range end");
                validate_block(stmt, s.body, "parallel loop body");
            },
        });
    }

    void validate_decl(const Decl& decl) {
        validate_node_header(decl);

        decl.visit(overloaded{
            [&](const FnDecl& d) {
                validate_name(decl, d.name, "function name");
                validate_params(decl, d.params);
                validate_attrs(decl, d.attrs);
                if (d.body) {
                    validate_block(decl, *d.body, "function body");
                }
                if (d.attrs.is_extern() && d.has_body()) {
                    report(decl, "extern function declarations cannot contain a body");
                } else if (!d.attrs.is_extern() && !d.has_body()) {
                    report(decl, "non-extern functions must contain a body");
                }
            },
            [&](const StructDecl& d) {
                validate_name(decl, d.name, "struct name");
                validate_attrs(decl, d.attrs);
                validate_unique_names(decl, d.fields, [](const FieldDecl& field) -> const std::string& {
                    return field.name;
                }, "field name");
            },
            [&](const ClassDecl& d) {
                validate_name(decl, d.name, "class name");
                validate_attrs(decl, d.attrs);
                validate_unique_names(decl, d.fields, [](const FieldDecl& field) -> const std::string& {
                    return field.name;
                }, "field name");
                validate_unique_names(decl, d.methods, [](const MethodDecl& method) -> const std::string& {
                    return method.name;
                }, "method name");
                for (const auto& method : d.methods) {
                    if (method.name.empty()) {
                        continue;
                    }
                    if (method.has_receiver()) {
                        for (const auto& param : method.params) {
                            if (param.name == "self") {
                                report(decl, std::format(
                                    "method '{}' uses an implicit self receiver and cannot also declare parameter 'self'",
                                    method.name));
                            }
                        }
                    }
                    validate_params(decl, method.params);
                    validate_block(decl, method.body, "method body");
                }
            },
            [&](const EnumDecl& d) {
                validate_name(decl, d.name, "enum name");
                validate_attrs(decl, d.attrs);
                validate_unique_names(decl, d.variants, [](const EnumVariantDecl& variant) -> const std::string& {
                    return variant.name;
                }, "enum variant name");
            },
            [&](const ConstDecl& d) {
                validate_name(decl, d.name, "const name");
                validate_attrs(decl, d.attrs);
                require_child(decl, d.value, "const value");
            },
            [&](const StaticDecl& d) {
                validate_name(decl, d.name, "static name");
                validate_attrs(decl, d.attrs);
            },
            [&](const ImportDecl& d) {
                validate_name(decl, d.path, "import path");
                if (d.alias) {
                    validate_name(decl, *d.alias, "import alias");
                }
            },
            [&](const TypeAliasDecl& d) {
                validate_name(decl, d.name, "type alias name");
            },
        });
    }
};

} // namespace detail

[[nodiscard]] inline std::vector<AstValidationError> validate_module(const Module& mod) {
    detail::AstValidator validator(mod);
    validator.validate_module();
    return std::move(validator).finish();
}

[[nodiscard]] inline std::vector<AstValidationError> validate_project_decl(
    const ProjectDecl& project,
    SourceLoc loc = {}) {
    if (loc.file == "<unknown>" && !project.name.empty()) {
        loc.file = project.name;
    }

    std::vector<AstValidationError> errors;
    auto report = [&](std::string message) {
        errors.push_back(AstValidationError{
            .message = std::move(message),
            .loc = loc,
            .node_id = {}
        });
    };

    if (project.name.empty()) {
        report("project name cannot be empty");
    }
    if (!project.mode.empty() && project.mode != "dll" && project.mode != "exe") {
        report(std::format("project mode must be 'dll' or 'exe', got '{}'", project.mode));
    }
    for (const auto& include : project.includes) {
        if (include.empty()) {
            report("project includes cannot contain empty paths");
        }
    }

    return errors;
}

} // namespace opus::ast
