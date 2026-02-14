#ifndef FLUX_AST_H
#define FLUX_AST_H

#include <memory>
#include <string>
#include <vector>

#include "lexer/token.h" // ✅ REQUIRED for TokenKind

namespace flux::ast {

enum class Visibility { None, Public, Private };

struct Node {
    virtual ~Node() = default;
    virtual std::unique_ptr<Node> clone() const = 0;
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
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<NumberExpr>(value);
    }
};

struct IdentifierExpr : Expr {
    std::string name;

    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<IdentifierExpr>(name);
    }
};

struct StringExpr : Expr {
    std::string value;

    explicit StringExpr(std::string v) : value(std::move(v)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<StringExpr>(value);
    }
};

struct CharExpr : Expr {
    std::string value;

    explicit CharExpr(std::string v) : value(std::move(v)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<CharExpr>(value);
    }
};

struct BoolExpr : Expr {
    bool value;

    explicit BoolExpr(bool v) : value(v) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<BoolExpr>(value);
    }
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpr(ExprPtr c, std::vector<ExprPtr> args)
        : callee(std::move(c)), arguments(std::move(args)) {}
    std::unique_ptr<Node> clone() const override {
        std::vector<ExprPtr> new_args;
        new_args.reserve(arguments.size());
        for (const auto& arg : arguments) {
            new_args.push_back(std::unique_ptr<Expr>(static_cast<Expr*>(arg->clone().release())));
        }
        return std::make_unique<CallExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(callee->clone().release())),
            std::move(new_args));
    }
};

struct BinaryExpr : Expr {
    TokenKind op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(TokenKind op, ExprPtr lhs, ExprPtr rhs)
        : op(op), left(std::move(lhs)), right(std::move(rhs)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<BinaryExpr>(
            op, std::unique_ptr<Expr>(static_cast<Expr*>(left->clone().release())),
            std::unique_ptr<Expr>(static_cast<Expr*>(right->clone().release())));
    }
};

struct UnaryExpr : Expr {
    TokenKind op;
    ExprPtr operand;
    bool is_mutable;

    UnaryExpr(TokenKind op, ExprPtr expr, bool is_mutable = false)
        : op(op), operand(std::move(expr)), is_mutable(is_mutable) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<UnaryExpr>(
            op, std::unique_ptr<Expr>(static_cast<Expr*>(operand->clone().release())), is_mutable);
    }
};

struct MoveExpr : Expr {
    ExprPtr operand;
    explicit MoveExpr(ExprPtr expr) : operand(std::move(expr)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<MoveExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(operand->clone().release())));
    }
};

struct CastExpr : Expr {
    ExprPtr expr;
    std::string target_type;
    CastExpr(ExprPtr e, std::string type) : expr(std::move(e)), target_type(std::move(type)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<CastExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(expr->clone().release())), target_type);
    }
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
    std::unique_ptr<Node> clone() const override {
        std::vector<FieldInit> new_fields;
        new_fields.reserve(fields.size());
        for (const auto& f : fields) {
            new_fields.push_back(
                {f.name, std::unique_ptr<Expr>(static_cast<Expr*>(f.value->clone().release()))});
        }
        return std::make_unique<StructLiteralExpr>(struct_name, std::move(new_fields));
    }
};

struct RangeExpr : Expr {
    ExprPtr start;
    ExprPtr end;
    bool inclusive; // .. vs ..=
    RangeExpr(ExprPtr s, ExprPtr e, bool incl = false)
        : start(std::move(s)), end(std::move(e)), inclusive(incl) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<RangeExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(start->clone().release())),
            std::unique_ptr<Expr>(static_cast<Expr*>(end->clone().release())), inclusive);
    }
};

struct MemberAccessExpr : Expr {
    ExprPtr object;
    std::string member;
    MemberAccessExpr(ExprPtr obj, std::string mem)
        : object(std::move(obj)), member(std::move(mem)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<MemberAccessExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(object->clone().release())), member);
    }
};

struct ErrorPropagationExpr : Expr {
    ExprPtr operand;
    explicit ErrorPropagationExpr(ExprPtr e) : operand(std::move(e)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<ErrorPropagationExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(operand->clone().release())));
    }
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
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<LambdaExpr>(
            params, return_type,
            std::unique_ptr<Expr>(static_cast<Expr*>(body->clone().release())));
    }
};

struct AwaitExpr : Expr {
    ExprPtr operand;
    explicit AwaitExpr(ExprPtr e) : operand(std::move(e)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<AwaitExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(operand->clone().release())));
    }
};

struct SpawnExpr : Expr {
    ExprPtr operand;
    explicit SpawnExpr(ExprPtr e) : operand(std::move(e)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<SpawnExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(operand->clone().release())));
    }
};

struct TupleExpr : Expr {
    std::vector<ExprPtr> elements;
    TupleExpr(std::vector<ExprPtr> elems) : elements(std::move(elems)) {}
    std::unique_ptr<Node> clone() const override {
        std::vector<ExprPtr> new_elems;
        new_elems.reserve(elements.size());
        for (const auto& e : elements)
            new_elems.push_back(std::unique_ptr<Expr>(static_cast<Expr*>(e->clone().release())));
        return std::make_unique<TupleExpr>(std::move(new_elems));
    }
};

struct ArrayExpr : Expr {
    std::vector<ExprPtr> elements;
    ArrayExpr(std::vector<ExprPtr> elems) : elements(std::move(elems)) {}
    std::unique_ptr<Node> clone() const override {
        std::vector<ExprPtr> new_elems;
        new_elems.reserve(elements.size());
        for (const auto& e : elements)
            new_elems.push_back(std::unique_ptr<Expr>(static_cast<Expr*>(e->clone().release())));
        return std::make_unique<ArrayExpr>(std::move(new_elems));
    }
};

struct SliceExpr : Expr {
    ExprPtr array;
    ExprPtr start;
    ExprPtr end;
    SliceExpr(ExprPtr arr, ExprPtr s, ExprPtr e)
        : array(std::move(arr)), start(std::move(s)), end(std::move(e)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<SliceExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(array->clone().release())),
            start ? std::unique_ptr<Expr>(static_cast<Expr*>(start->clone().release())) : nullptr,
            end ? std::unique_ptr<Expr>(static_cast<Expr*>(end->clone().release())) : nullptr);
    }
};

struct IndexExpr : Expr {
    ExprPtr array;
    ExprPtr index;
    IndexExpr(ExprPtr arr, ExprPtr idx) : array(std::move(arr)), index(std::move(idx)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<IndexExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(array->clone().release())),
            std::unique_ptr<Expr>(static_cast<Expr*>(index->clone().release())));
    }
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

    std::unique_ptr<Node> clone() const override {
        if (tuple_names.empty()) {
            return std::make_unique<LetStmt>(
                name, type_name, is_mutable, is_const,
                std::unique_ptr<Expr>(static_cast<Expr*>(initializer->clone().release())));
        } else {
            return std::make_unique<LetStmt>(
                tuple_names, type_name, is_mutable, is_const,
                std::unique_ptr<Expr>(static_cast<Expr*>(initializer->clone().release())));
        }
    }
};

struct ReturnStmt : Stmt {
    ExprPtr expression;

    explicit ReturnStmt(ExprPtr expr) : expression(std::move(expr)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<ReturnStmt>(
            expression ? std::unique_ptr<Expr>(static_cast<Expr*>(expression->clone().release()))
                       : nullptr);
    }
};

struct ExprStmt : Stmt {
    ExprPtr expression;

    explicit ExprStmt(ExprPtr expr) : expression(std::move(expr)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<ExprStmt>(
            std::unique_ptr<Expr>(static_cast<Expr*>(expression->clone().release())));
    }
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr then_branch;
    StmtPtr else_branch; // can be null

    IfStmt(ExprPtr cond, StmtPtr then_b, StmtPtr else_b)
        : condition(std::move(cond)), then_branch(std::move(then_b)),
          else_branch(std::move(else_b)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<IfStmt>(
            std::unique_ptr<Expr>(static_cast<Expr*>(condition->clone().release())),
            std::unique_ptr<Stmt>(static_cast<Stmt*>(then_branch->clone().release())),
            else_branch ? std::unique_ptr<Stmt>(static_cast<Stmt*>(else_branch->clone().release()))
                        : nullptr);
    }
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;

    WhileStmt(ExprPtr cond, StmtPtr body) : condition(std::move(cond)), body(std::move(body)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<WhileStmt>(
            std::unique_ptr<Expr>(static_cast<Expr*>(condition->clone().release())),
            std::unique_ptr<Stmt>(static_cast<Stmt*>(body->clone().release())));
    }
};

struct ForStmt : Stmt {
    std::string variable;
    std::string var_type; // optional type annotation (empty if not provided)
    ExprPtr iterable;
    StmtPtr body;

    ForStmt(std::string var, std::string type, ExprPtr iter, StmtPtr body)
        : variable(std::move(var)), var_type(std::move(type)), iterable(std::move(iter)),
          body(std::move(body)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<ForStmt>(
            variable, var_type,
            std::unique_ptr<Expr>(static_cast<Expr*>(iterable->clone().release())),
            std::unique_ptr<Stmt>(static_cast<Stmt*>(body->clone().release())));
    }
};

struct LoopStmt : Stmt {
    StmtPtr body;
    explicit LoopStmt(StmtPtr b) : body(std::move(b)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<LoopStmt>(
            std::unique_ptr<Stmt>(static_cast<Stmt*>(body->clone().release())));
    }
};

struct BreakStmt : Stmt {
    ExprPtr value; // optional
    explicit BreakStmt(ExprPtr v = nullptr) : value(std::move(v)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<BreakStmt>(
            value ? std::unique_ptr<Expr>(static_cast<Expr*>(value->clone().release())) : nullptr);
    }
};
struct ContinueStmt : Stmt {
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<ContinueStmt>();
    }
};

struct AssignStmt : Stmt {
    ExprPtr target;
    ExprPtr value;
    TokenKind op; // Assign, PlusAssign, MinusAssign, etc.

    AssignStmt(ExprPtr target, ExprPtr value, TokenKind op = TokenKind::Assign)
        : target(std::move(target)), value(std::move(value)), op(op) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<AssignStmt>(
            std::unique_ptr<Expr>(static_cast<Expr*>(target->clone().release())),
            std::unique_ptr<Expr>(static_cast<Expr*>(value->clone().release())), op);
    }
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
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<LiteralPattern>(
            std::unique_ptr<Expr>(static_cast<Expr*>(literal->clone().release())));
    }
};

struct IdentifierPattern : Pattern {
    std::string name;
    explicit IdentifierPattern(std::string n) : name(std::move(n)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<IdentifierPattern>(name);
    }
};

struct WildcardPattern : Pattern {
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<WildcardPattern>();
    }
};

struct VariantPattern : Pattern {
    std::string variant_name;
    std::vector<PatternPtr> sub_patterns;

    VariantPattern(std::string name, std::vector<PatternPtr> sub)
        : variant_name(std::move(name)), sub_patterns(std::move(sub)) {}
    std::unique_ptr<Node> clone() const override {
        std::vector<PatternPtr> new_subs;
        new_subs.reserve(sub_patterns.size());
        for (const auto& p : sub_patterns)
            new_subs.push_back(
                std::unique_ptr<Pattern>(static_cast<Pattern*>(p->clone().release())));
        return std::make_unique<VariantPattern>(variant_name, std::move(new_subs));
    }
};

struct TuplePattern : Pattern {
    std::vector<PatternPtr> elements;
    TuplePattern(std::vector<PatternPtr> elems) : elements(std::move(elems)) {}
    std::unique_ptr<Node> clone() const override {
        std::vector<PatternPtr> new_elems;
        new_elems.reserve(elements.size());
        for (const auto& e : elements)
            new_elems.push_back(
                std::unique_ptr<Pattern>(static_cast<Pattern*>(e->clone().release())));
        return std::make_unique<TuplePattern>(std::move(new_elems));
    }
};

struct FieldPattern {
    std::string field_name;
    PatternPtr pattern;
};

struct StructPattern : Pattern {
    std::string struct_name;
    std::vector<FieldPattern> fields;

    StructPattern(std::string name, std::vector<FieldPattern> fds)
        : struct_name(std::move(name)), fields(std::move(fds)) {}
    std::unique_ptr<Node> clone() const override {
        std::vector<FieldPattern> new_fields;
        new_fields.reserve(fields.size());
        for (const auto& f : fields) {
            new_fields.push_back({f.field_name, std::unique_ptr<Pattern>(static_cast<Pattern*>(
                                                    f.pattern->clone().release()))});
        }
        return std::make_unique<StructPattern>(struct_name, std::move(new_fields));
    }
};

struct RangePattern : Pattern {
    ExprPtr start;
    ExprPtr end;
    bool is_inclusive;

    RangePattern(ExprPtr s, ExprPtr e, bool inc)
        : start(std::move(s)), end(std::move(e)), is_inclusive(inc) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<RangePattern>(
            std::unique_ptr<Expr>(static_cast<Expr*>(start->clone().release())),
            std::unique_ptr<Expr>(static_cast<Expr*>(end->clone().release())), is_inclusive);
    }
};

struct OrPattern : Pattern {
    std::vector<PatternPtr> alternatives;

    OrPattern(std::vector<PatternPtr> alts) : alternatives(std::move(alts)) {}
    std::unique_ptr<Node> clone() const override {
        std::vector<PatternPtr> new_alts;
        new_alts.reserve(alternatives.size());
        for (const auto& a : alternatives)
            new_alts.push_back(
                std::unique_ptr<Pattern>(static_cast<Pattern*>(a->clone().release())));
        return std::make_unique<OrPattern>(std::move(new_alts));
    }
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
    std::unique_ptr<Node> clone() const override {
        std::vector<MatchArm> new_arms;
        for (const auto& arm : arms) {
            new_arms.push_back(
                {std::unique_ptr<Pattern>(static_cast<Pattern*>(arm.pattern->clone().release())),
                 arm.guard ? std::unique_ptr<Expr>(static_cast<Expr*>(arm.guard->clone().release()))
                           : nullptr,
                 std::unique_ptr<Stmt>(static_cast<Stmt*>(arm.body->clone().release()))});
        }
        return std::make_unique<MatchStmt>(
            std::unique_ptr<Expr>(static_cast<Expr*>(expression->clone().release())),
            std::move(new_arms));
    }
};

/* =======================
   Blocks
   ======================= */

struct Block : Node {
    std::vector<StmtPtr> statements;
    std::unique_ptr<Node> clone() const override {
        auto new_block = std::make_unique<Block>();
        for (const auto& stmt : statements) {
            new_block->statements.push_back(
                std::unique_ptr<Stmt>(static_cast<Stmt*>(stmt->clone().release())));
        }
        return new_block;
    }
};

struct BlockStmt : Stmt {
    Block block; // stored by value
    std::unique_ptr<Node> clone() const override {
        auto new_stmt = std::make_unique<BlockStmt>();
        // Manually clone the block's content since Block is not copyable (contains unique_ptrs)
        for (const auto& stmt : block.statements) {
            new_stmt->block.statements.push_back(
                std::unique_ptr<Stmt>(static_cast<Stmt*>(stmt->clone().release())));
        }
        return new_stmt;
    }
};

/* =======================
   Functions
   ======================= */

struct Param {
    std::string name;
    std::string type;
};

struct AssociatedType : Node {
    std::string name;
    std::string default_type; // Optional for trait declarations

    AssociatedType(std::string n, std::string d = "")
        : name(std::move(n)), default_type(std::move(d)) {}
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<AssociatedType>(name, default_type);
    }
};

struct FunctionDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Param> params;
    std::string return_type;
    Block body;
    Visibility visibility = Visibility::None;
    bool is_async = false;
    bool has_body = false;
    std::string where_clause; // raw string for now

    std::unique_ptr<Node> clone() const override {
        auto new_fn = std::make_unique<FunctionDecl>();
        new_fn->name = name;
        new_fn->type_params = type_params;
        new_fn->params = params;
        new_fn->return_type = return_type;
        for (const auto& stmt : body.statements) {
            new_fn->body.statements.push_back(
                std::unique_ptr<Stmt>(static_cast<Stmt*>(stmt->clone().release())));
        }
        new_fn->visibility = visibility;
        new_fn->is_async = is_async;
        new_fn->has_body = has_body;
        new_fn->where_clause = where_clause;
        return new_fn;
    }
};

/* =======================
   Structs & Enums
   ======================= */

struct Field {
    std::string name;
    std::string type;
    Visibility visibility = Visibility::None;
};

struct StructDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Field> fields;
    Visibility visibility = Visibility::None;
    std::string where_clause;

    // Default constructor for vector/etc
    StructDecl() = default;

    StructDecl(std::string name, std::vector<std::string> type_params, std::vector<Field> fields)
        : name(std::move(name)), type_params(std::move(type_params)), fields(std::move(fields)) {}

    std::unique_ptr<Node> clone() const override {
        auto s = std::make_unique<StructDecl>(name, type_params, fields);
        s->visibility = visibility;
        s->where_clause = where_clause;
        return s;
    }
};

struct ClassDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Field> fields;
    Visibility visibility = Visibility::None;
    std::string where_clause;

    ClassDecl(std::string name, std::vector<std::string> type_params, std::vector<Field> fields)
        : name(std::move(name)), type_params(std::move(type_params)), fields(std::move(fields)) {}

    std::unique_ptr<Node> clone() const override {
        auto c = std::make_unique<ClassDecl>(name, type_params, fields);
        c->visibility = visibility;
        c->where_clause = where_clause;
        return c;
    }
};

struct Variant {
    std::string name;
    std::vector<std::string> types; // tuple variants for now
};

struct EnumDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Variant> variants;
    Visibility visibility = Visibility::None;
    std::string where_clause;

    EnumDecl(std::string name, std::vector<std::string> type_params, std::vector<Variant> variants)
        : name(std::move(name)), type_params(std::move(type_params)),
          variants(std::move(variants)) {}

    std::unique_ptr<Node> clone() const override {
        auto e = std::make_unique<EnumDecl>(name, type_params, variants);
        e->visibility = visibility;
        e->where_clause = where_clause;
        return e;
    }
};

struct ImplBlock : Node {
    std::vector<std::string> type_params;
    std::string target_name;
    std::string trait_name; // empty if not a trait impl
    std::vector<FunctionDecl> methods;
    std::vector<AssociatedType> associated_types;
    std::string where_clause; // raw string for now

    ImplBlock(std::vector<std::string> type_params, std::string target,
              std::vector<FunctionDecl> methods)
        : type_params(std::move(type_params)), target_name(std::move(target)),
          methods(std::move(methods)) {}

    std::unique_ptr<Node> clone() const override {
        std::vector<FunctionDecl> new_methods;
        new_methods.reserve(methods.size());
        for (const auto& m : methods) {
            auto ptr = m.clone();
            new_methods.push_back(std::move(*static_cast<FunctionDecl*>(ptr.get())));
        }
        auto i = std::make_unique<ImplBlock>(type_params, target_name, std::move(new_methods));
        i->trait_name = trait_name;
        for (const auto& at : associated_types) {
            i->associated_types.emplace_back(at.name, at.default_type);
        }
        i->where_clause = where_clause;
        return i;
    }
};

struct TraitDecl : Node {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<FunctionDecl> methods; // potentially just signatures later
    std::vector<AssociatedType> associated_types;
    Visibility visibility = Visibility::None;
    std::string where_clause; // raw string for now

    TraitDecl(std::string name, std::vector<std::string> type_params,
              std::vector<FunctionDecl> methods)
        : name(std::move(name)), type_params(std::move(type_params)), methods(std::move(methods)) {}

    std::unique_ptr<Node> clone() const override {
        std::vector<FunctionDecl> new_methods;
        new_methods.reserve(methods.size());
        for (const auto& m : methods) {
            auto ptr = m.clone();
            new_methods.push_back(std::move(*static_cast<FunctionDecl*>(ptr.get())));
        }
        auto t = std::make_unique<TraitDecl>(name, type_params, std::move(new_methods));
        t->visibility = visibility;
        for (const auto& at : associated_types) {
            t->associated_types.emplace_back(at.name, at.default_type);
        }
        t->where_clause = where_clause;
        return t;
    }
};

struct TypeAlias : Node {
    std::string name;
    std::string target_type;
    Visibility visibility = Visibility::None;

    TypeAlias(std::string name, std::string target)
        : name(std::move(name)), target_type(std::move(target)) {}

    std::unique_ptr<Node> clone() const override {
        auto ta = std::make_unique<TypeAlias>(name, target_type);
        ta->visibility = visibility;
        return ta;
    }
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
    std::unique_ptr<Node> clone() const override {
        return std::make_unique<Import>(module_path);
    }
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

    std::unique_ptr<Node> clone() const override {
        auto m = std::make_unique<Module>();
        m->name = name;
        for (const auto& imp : imports)
            m->imports.push_back(Import(imp.module_path));
        for (const auto& f : functions) {
            auto ptr = f.clone();
            m->functions.push_back(std::move(*static_cast<FunctionDecl*>(ptr.get())));
        }
        for (const auto& s : structs) {
            auto ptr = s.clone();
            m->structs.push_back(std::move(*static_cast<StructDecl*>(ptr.get())));
        }
        for (const auto& c : classes) {
            auto ptr = c.clone();
            m->classes.push_back(std::move(*static_cast<ClassDecl*>(ptr.get())));
        }
        for (const auto& e : enums) {
            auto ptr = e.clone();
            m->enums.push_back(std::move(*static_cast<EnumDecl*>(ptr.get())));
        }
        for (const auto& i : impls) {
            auto ptr = i.clone();
            m->impls.push_back(std::move(*static_cast<ImplBlock*>(ptr.get())));
        }
        for (const auto& t : traits) {
            auto ptr = t.clone();
            m->traits.push_back(std::move(*static_cast<TraitDecl*>(ptr.get())));
        }
        for (const auto& ta : type_aliases) {
            auto ptr = ta.clone();
            m->type_aliases.push_back(std::move(*static_cast<TypeAlias*>(ptr.get())));
        }
        return m;
    }
};
} // namespace flux::ast

#endif // FLUX_AST_H
