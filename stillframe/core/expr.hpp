// Safe expression language used inside scripts. No I/O, no loops, no user functions:
// a script (or a downloaded game) can only compute values from its own variables.
#pragma once
#include "json.hpp"
#include <functional>
#include <memory>

namespace sf {

struct ExprNode;
using Expr = std::shared_ptr<const ExprNode>;

// Parses `text`. Throws Error with a readable message on bad syntax.
Expr parse_expr(std::string_view text);

struct EvalContext {
    Json* vars = nullptr;                              // variable root (an object)
    uint64_t* rng = nullptr;                           // saved with the game so rolls are reproducible after load
    std::function<bool(const std::string&)> seen;      // seen("label")
};

Json eval(const Expr& e, EvalContext& ctx);
bool truthy(const Json& v);
// Applies a binary operator (+ - * / // % == != < <= > >= in) to two values.
Json apply_op(const std::string& op, const Json& l, const Json& r);
// Assignment through an l-value expression (name, a.b, a[i]); creates intermediate objects.
void assign(const Expr& lhs, EvalContext& ctx, Json value);
bool is_lvalue(const Expr& e);
// Names of the root variables an expression reads (for validation tooling).
void collect_roots(const Expr& e, std::vector<std::string>& out);

// xorshift64* step; returns a value in [0, 2^64).
uint64_t rng_next(uint64_t& state);

} // namespace sf
