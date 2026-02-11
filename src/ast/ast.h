#ifndef FLUX_AST_H
#define FLUX_AST_H

#include <memory>
#include <string>
#include <vector>

#include "lexer/token.h" // ✅ REQUIRED for TokenKind

namespace flux::ast {
struct Node {
    virtual ~Node() = default;
};

/* =======================
        Expressions
======================= */

struct Expr : Node {
    ~Expr() override = default;
};

using ExprPtr = std::unique_ptr<Expr>;

struct NumberExpr : Expr {
    std::string value;

    explicit NumberExpr(std::string v) : value(std::move(v)) {}
};

struct IdentifierExpr : Expr {
    std::string name;

    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
};

struct StringExpr : Expr {
    std::string value;

    explicit StringExpr(std::string v) : value(std::move(v)) {}
};

struct CharExpr : Expr {
    std::string value;

    explicit CharExpr(std::string v) : value(std::move(v)) {}
};

struct BoolExpr : Expr {
    bool value;

    explicit BoolExpr(bool v) : value(v) {}
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpr(ExprPtr c, std::vector<ExprPtr> args)
        : callee(std::move(c)), arguments(std::move(args)) {}
};

struct BinaryExpr : Expr {
    TokenKind op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(TokenKind op, ExprPtr lhs, ExprPtr rhs)
        : op(op), left(std::move(lhs)), right(std::move(rhs)) {}
};

struct UnaryExpr : Expr {
    TokenKind op;
    ExprPtr operand;
    bool is_mutable;

    UnaryExpr(TokenKind op, ExprPtr expr, bool is_mutable = false)
        : op(op), operand(std::move(expr)), is_mutable(is_mutable) {}
};

struct MoveExpr : Expr {
    ExprPtr operand;
    explicit MoveExpr(ExprPtr expr) : operand(std::move(expr)) {}
};

struct CastExpr : Expr {
    ExprPtr expr;
    std::string target_type;
    CastExpr(ExprPtr e, std::string type) : expr(std::move(e)), target_type(std::move(type)) {}
};

struct FieldInit {
    std::string name;
    ExprPtr value;
};

struct StructLiteralExpr : Expr {
    std::string struct_name;
    std::vector<FieldInit> fields;

    StructLiteralExpr(std::string name, std::vector<FieldInit> flds)
        : struct_name(std::move(name)), fields(std::move(flds)) {}
};

struct RangeExpr : Expr {
    ExprPtr start;
    ExprPtr end;
    bool inclusive; // .. vs ..=
    RangeExpr(ExprPtr s, ExprPtr e, bool incl = false)
        : start(std::move(s)), end(std::move(e)), inclusive(incl) {}
};

struct ErrorPropagationExpr : Expr {
    ExprPtr operand;
    explicit ErrorPropagationExpr(ExprPtr e) : operand(std::move(e)) {}
};

struct LambdaExpr : Expr {
    struct Param {
        std::string name;
        std::string type;
        Param(std::string n, std::string t) : name(std::move(n)), type(std::move(t)) {}
    };
    std::vector<Param> params;
    std::string return_type;
    ExprPtr body;
    LambdaExpr(std::vector<Param> p, std::string ret, ExprPtr b)
        : params(std::move(p)), return_type(std::move(ret)), body(std::move(b)) {}
};

struct AwaitExpr : Expr {
    ExprPtr operand;
    explicit AwaitExpr(ExprPtr e) : operand(std::move(e)) {}
};

struct SpawnExpr : Expr {
    ExprPtr operand;
    explicit SpawnExpr(ExprPtr e) : operand(std::move(e)) {}
};

struct TupleExpr : Expr {
    std::vector<ExprPtr> elements;
    TupleExpr(std::vector<ExprPtr> elems) : elements(std::move(elems)) {}
};

struct ArrayExpr : Expr {
    std::vector<ExprPtr> elements;
    ArrayExpr(std::vector<ExprPtr> elems) : elements(std::move(elems)) {}
};

struct SliceExpr : Expr {
    ExprPtr array;
    ExprPtr start;
    ExprPtr end;
    SliceExpr(ExprPtr arr, ExprPtr s, ExprPtr e)
        : array(std::move(arr)), start(std::move(s)), end(std::move(e)) {}
};

/* =======================
   Statements
   ======================= */

struct Stmt : Node {
    ~Stmt() override = default;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct LetStmt : Stmt {
    std::string name;
    std::vector<std::string> tuple_names; // for tuple destructuring
    std::string type_name;
    bool is_mutable;
    bool is_const;
    ExprPtr initializer;

    // Single variable
    LetStmt(std::string name, std::string type, bool mut, bool is_const, ExprPtr init)
        : name(std::move(name)), type_name(std::move(type)), is_mutable(mut), is_const(is_const),
          initializer(std::move(init)) {}

    // Tuple destructuring
    LetStmt(std::vector<std::string> tuple_names, std::string type, bool mut, bool is_const,
            ExprPtr init)
        : name(""), tuple_names(std::move(tuple_names)), type_name(std::move(type)),
          is_mutable(mut), is_const(is_const), initializer(std::move(init)) {}
};

struct ReturnStmt : Stmt {
    ExprPtr expression;

    explicit ReturnStmt(ExprPtr expr) : expression(std::move(expr)) {}
};

struct ExprStmt : Stmt {
    ExprPtr expression;

    explicit ExprStmt(ExprPtr expr) : expression(std::move(expr)) {}
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr then_branch;
    StmtPtr else_branch; // can be null

    IfStmt(ExprPtr cond, StmtPtr then_b, StmtPtr else_b)
        : condition(std::move(cond)), then_branch(std::move(then_b)),
          else_branch(std::move(else_b)) {}
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;

    WhileStmt(ExprPtr cond, StmtPtr body) : condition(std::move(cond)), body(std::move(body)) {}
};

struct ForStmt : Stmt {
    std::string variable;
    std::string var_type; // optional type annotation (empty if not provided)
    ExprPtr iterable;
    StmtPtr body;

    ForStmt(std::string var, std::string type, ExprPtr iter, StmtPtr body)
        : variable(std::move(var)), var_type(std::move(type)), iterable(std::move(iter)),
          body(std::move(body)) {}
};

struct LoopStmt : Stmt {
    StmtPtr body;
    explicit LoopStmt(StmtPtr b) : body(std::move(b)) {}
};

struct BreakStmt : Stmt {};
struct ContinueStmt : Stmt {};

struct AssignStmt : Stmt {
    ExprPtr target;
    ExprPtr value;
    TokenKind op; // Assign, PlusAssign, MinusAssign, etc.

    AssignStmt(ExprPtr target, ExprPtr value, TokenKind op = TokenKind::Assign)
        : target(std::move(target)), value(std::move(value)), op(op) {}
};

/* =======================
   Patterns
   ======================= */

struct Pattern : Node {
    virtual ~Pattern() override = default;
};

using PatternPtr = std::unique_ptr<Pattern>;

struct LiteralPattern : Pattern {
    ExprPtr literal;
    explicit LiteralPattern(ExprPtr lit) : literal(std::move(lit)) {}
};

struct IdentifierPattern : Pattern {
    std::string name;
    explicit IdentifierPattern(std::string n) : name(std::move(n)) {}
};

struct WildcardPattern : Pattern {};

struct VariantPattern : Pattern {
    std::string variant_name;
    std::vector<PatternPtr> sub_patterns;

    VariantPattern(std::string name, std::vector<PatternPtr> sub)
        : variant_name(std::move(name)), sub_patterns(std::move(sub)) {}
};

struct MatchArm {
    PatternPtr pattern;
    ExprPtr guard; // optional match guard (if condition)
    StmtPtr body;
};

struct MatchStmt : Stmt {
    ExprPtr expression;
    std::vector<MatchArm> arms;

    MatchStmt(ExprPtr expr, std::vector<MatchArm> match_arms)
        : expression(std::move(expr)), arms(std::move(match_arms)) {}
};

/* =======================
   Blocks
   ======================= */

struct Block : Node {
    std::vector<StmtPtr> statements;
};

struct BlockStmt : Stmt {
    Block block;
};

/* =======================
   Functions
   ======================= */

struct Param {
    std::string name;
    std::string type;
};

struct FunctionDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Param> params;
    std::string return_type;
    Block body;
    bool is_public = false;
    bool is_async = false;
    std::string where_clause; // raw string for now
};

/* =======================
   Structs & Enums
   ======================= */

struct Field {
    std::string name;
    std::string type;
    std::string visibility; // "", "pub", "public", "private"
};

struct StructDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Field> fields;
    bool is_public = false;

    StructDecl(std::string name, std::vector<std::string> type_params, std::vector<Field> fields)
        : name(std::move(name)), type_params(std::move(type_params)), fields(std::move(fields)) {}
};

struct ClassDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Field> fields;
    bool is_public = false;

    ClassDecl(std::string name, std::vector<std::string> type_params, std::vector<Field> fields)
        : name(std::move(name)), type_params(std::move(type_params)), fields(std::move(fields)) {}
};

struct Variant {
    std::string name;
    std::vector<std::string> types; // tuple variants for now
};

struct EnumDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Variant> variants;
    bool is_public = false;

    EnumDecl(std::string name, std::vector<std::string> type_params, std::vector<Variant> variants)
        : name(std::move(name)), type_params(std::move(type_params)),
          variants(std::move(variants)) {}
};

struct ImplBlock : Node {
    std::vector<std::string> type_params;
    std::string target_name;
    std::string trait_name; // empty if not a trait impl
    std::vector<FunctionDecl> methods;
    std::string where_clause; // raw string for now

    ImplBlock(std::vector<std::string> type_params, std::string target,
              std::vector<FunctionDecl> methods)
        : type_params(std::move(type_params)), target_name(std::move(target)),
          methods(std::move(methods)) {}
};

struct TraitDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<FunctionDecl> methods; // potentially just signatures later
    bool is_public = false;
    std::string where_clause; // raw string for now

    TraitDecl(std::string name, std::vector<std::string> type_params,
              std::vector<FunctionDecl> methods)
        : name(std::move(name)), type_params(std::move(type_params)), methods(std::move(methods)) {}
};

struct TypeAlias : Node {
    std::string name;
    std::string target_type;
    bool is_public = false;

    TypeAlias(std::string name, std::string target)
        : name(std::move(name)), target_type(std::move(target)) {}
};

/* =======================
   Annotations
   ======================= */

struct Annotation {
    std::string name;  // e.g. "test", "deprecated", "doc", "inline"
    std::string value; // optional value
};

/* =======================
   Imports
   ======================= */

struct Import : Node {
    std::string module_path;

    explicit Import(std::string path) : module_path(std::move(path)) {}
};

/* =======================
   Module
   ======================= */

struct Module : Node {
    std::string name;
    std::vector<Import> imports;
    std::vector<FunctionDecl> functions;
    std::vector<StructDecl> structs;
    std::vector<ClassDecl> classes;
    std::vector<EnumDecl> enums;
    std::vector<ImplBlock> impls;
    std::vector<TraitDecl> traits;
    std::vector<TypeAlias> type_aliases;
};
} // namespace flux::ast

#endif // FLUX_AST_H
