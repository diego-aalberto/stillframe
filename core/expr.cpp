#include "expr.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace sf {

enum class K { Num, Str, Bool, Null, Var, Member, Index, Call, Unary, Binary, List };

struct ExprNode {
    K kind;
    Json value;                  // Num/Str/Bool literal
    std::string name;            // Var/Member name, Call function name, operator text
    std::vector<Expr> kids;      // operands / args / list items
};

namespace {
Expr mk(K k, std::string name = {}, Json v = {}, std::vector<Expr> kids = {}) {
    auto n = std::make_shared<ExprNode>();
    n->kind = k; n->name = std::move(name); n->value = std::move(v); n->kids = std::move(kids);
    return n;
}

struct Tok { enum T { Num, Str, Id, Op, End } t; std::string s; Json num; };

struct Lexer {
    std::string_view src;
    size_t i = 0;
    [[noreturn]] void fail(const std::string& m) const { throw Error("expression '" + std::string(src) + "': " + m); }
    Tok next() {
        while (i < src.size() && (src[i] == ' ' || src[i] == '\t')) ++i;
        if (i >= src.size()) return {Tok::End, "", {}};
        char c = src[i];
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[i + 1])))) {
            size_t st = i; bool real = false;
            while (i < src.size() && (std::isdigit(static_cast<unsigned char>(src[i])) || src[i] == '.')) { if (src[i] == '.') real = true; ++i; }
            std::string t(src.substr(st, i - st));
            if (real) return {Tok::Num, t, Json(std::strtod(t.c_str(), nullptr))};
            return {Tok::Num, t, Json(static_cast<int64_t>(std::strtoll(t.c_str(), nullptr, 10)))};
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t st = i;
            while (i < src.size() && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_')) ++i;
            return {Tok::Id, std::string(src.substr(st, i - st)), {}};
        }
        if (c == '"' || c == '\'') {
            char q = c; ++i;
            std::string out;
            while (true) {
                if (i >= src.size()) fail("unterminated string");
                char d = src[i++];
                if (d == q) break;
                if (d == '\\' && i < src.size()) {
                    char e = src[i++];
                    out += e == 'n' ? '\n' : e == 't' ? '\t' : e;
                } else out += d;
            }
            return {Tok::Str, out, {}};
        }
        static const char* two[] = {"==", "!=", "<=", ">=", "//"};
        for (auto t : two) if (src.substr(i, 2) == t) { i += 2; return {Tok::Op, t, {}}; }
        if (std::string("+-*/%<>()[].,!").find(c) != std::string::npos) { ++i; return {Tok::Op, std::string(1, c), {}}; }
        fail(std::string("unexpected character '") + c + "'");
    }
};

struct Parser {
    Lexer lx;
    Tok cur;
    explicit Parser(std::string_view s) : lx{s} { cur = lx.next(); }
    void adv() { cur = lx.next(); }
    bool op(const char* o) const { return cur.t == Tok::Op && cur.s == o; }
    bool kw(const char* k) const { return cur.t == Tok::Id && cur.s == k; }
    void expect(const char* o) { if (!op(o)) lx.fail(std::string("expected '") + o + "'"); adv(); }

    Expr parse() {
        Expr e = or_();
        if (cur.t != Tok::End) lx.fail("unexpected '" + cur.s + "'");
        return e;
    }
    Expr or_() { Expr l = and_(); while (kw("or")) { adv(); l = mk(K::Binary, "or", {}, {l, and_()}); } return l; }
    Expr and_() { Expr l = not_(); while (kw("and")) { adv(); l = mk(K::Binary, "and", {}, {l, not_()}); } return l; }
    Expr not_() { if (kw("not") || op("!")) { adv(); return mk(K::Unary, "not", {}, {not_()}); } return cmp(); }
    Expr cmp() {
        Expr l = add();
        for (const char* o : {"==", "!=", "<=", ">=", "<", ">"})
            if (op(o)) { adv(); return mk(K::Binary, o, {}, {l, add()}); }
        if (kw("in")) { adv(); return mk(K::Binary, "in", {}, {l, add()}); }
        if (kw("not")) { // "a not in b"
            Lexer save = lx; Tok t = cur;
            adv();
            if (kw("in")) { adv(); return mk(K::Unary, "not", {}, {mk(K::Binary, "in", {}, {l, add()})}); }
            lx = save; cur = t;
        }
        return l;
    }
    Expr add() { Expr l = mul(); while (op("+") || op("-")) { std::string o = cur.s; adv(); l = mk(K::Binary, o, {}, {l, mul()}); } return l; }
    Expr mul() { Expr l = unary(); while (op("*") || op("/") || op("%") || op("//")) { std::string o = cur.s; adv(); l = mk(K::Binary, o, {}, {l, unary()}); } return l; }
    Expr unary() { if (op("-")) { adv(); return mk(K::Unary, "-", {}, {unary()}); } return postfix(); }
    Expr postfix() {
        Expr e = primary();
        while (true) {
            if (op(".")) {
                adv();
                if (cur.t != Tok::Id) lx.fail("expected a name after '.'");
                e = mk(K::Member, cur.s, {}, {e}); adv();
            } else if (op("[")) { adv(); Expr i = or_(); expect("]"); e = mk(K::Index, "", {}, {e, i}); }
            else if (op("(")) {
                if (e->kind != K::Var) lx.fail("only named functions can be called");
                adv();
                std::vector<Expr> args;
                if (!op(")")) { do args.push_back(or_()); while (op(",") && (adv(), true)); }
                expect(")");
                e = mk(K::Call, e->name, {}, std::move(args));
            } else break;
        }
        return e;
    }
    Expr primary() {
        if (cur.t == Tok::Num) { Json v = cur.num; adv(); return mk(K::Num, "", v); }
        if (cur.t == Tok::Str) { Json v = cur.s; adv(); return mk(K::Str, "", v); }
        if (cur.t == Tok::Id) {
            std::string n = cur.s; adv();
            if (n == "true" || n == "True") return mk(K::Bool, "", true);
            if (n == "false" || n == "False") return mk(K::Bool, "", false);
            if (n == "none" || n == "None" || n == "null") return mk(K::Null);
            if (n == "and" || n == "or" || n == "not" || n == "in") lx.fail("unexpected '" + n + "'");
            return mk(K::Var, n);
        }
        if (op("(")) { adv(); Expr e = or_(); expect(")"); return e; }
        if (op("[")) {
            adv();
            std::vector<Expr> items;
            if (!op("]")) { do items.push_back(or_()); while (op(",") && (adv(), true)); }
            expect("]");
            return mk(K::List, "", {}, std::move(items));
        }
        lx.fail(cur.t == Tok::End ? "unexpected end" : "unexpected '" + cur.s + "'");
    }
};

[[noreturn]] void bad(const std::string& m) { throw Error("script: " + m); }

double num_of(const Json& v, const char* what) {
    if (v.is_number()) return v.as_num();
    if (v.is_null()) return 0;
    if (v.is_bool()) return v.as_bool() ? 1 : 0;
    bad(std::string("expected a number for ") + what);
}
bool both_int(const Json& a, const Json& b) { return (a.is_int() || a.is_null() || a.is_bool()) && (b.is_int() || b.is_null() || b.is_bool()); }
int64_t int_of(const Json& v) { return v.is_bool() ? (v.as_bool() ? 1 : 0) : v.as_int(); }

std::string to_text(const Json& v) {
    if (v.is_string()) return v.as_str();
    if (v.is_null()) return "";
    return v.dump();
}

int compare(const Json& a, const Json& b) {
    if (a.is_string() && b.is_string()) return a.as_str().compare(b.as_str());
    if ((a.is_number() || a.is_null() || a.is_bool()) && (b.is_number() || b.is_null() || b.is_bool())) {
        double x = num_of(a, "comparison"), y = num_of(b, "comparison");
        return x < y ? -1 : x > y ? 1 : 0;
    }
    bad("cannot order these values");
}

Json call(const std::string& fn, std::vector<Json>& a, EvalContext& ctx) {
    auto need = [&](size_t lo, size_t hi) { if (a.size() < lo || a.size() > hi) bad(fn + "() takes " + std::to_string(lo) + (lo == hi ? "" : "-" + std::to_string(hi)) + " argument(s)"); };
    if (fn == "min" || fn == "max") {
        if (a.empty()) bad(fn + "() needs arguments");
        Json best = a[0];
        for (size_t i = 1; i < a.size(); ++i) { int c = compare(a[i], best); if (fn == "min" ? c < 0 : c > 0) best = a[i]; }
        return best;
    }
    if (fn == "clamp") { need(3, 3); if (compare(a[0], a[1]) < 0) return a[1]; if (compare(a[0], a[2]) > 0) return a[2]; return a[0]; }
    if (fn == "abs") { need(1, 1); return a[0].is_int() ? Json(static_cast<int64_t>(std::llabs(a[0].as_int()))) : Json(std::fabs(num_of(a[0], "abs"))); }
    if (fn == "round") { need(1, 1); return Json(static_cast<int64_t>(std::llround(num_of(a[0], "round")))); }
    if (fn == "floor") { need(1, 1); return Json(static_cast<int64_t>(std::floor(num_of(a[0], "floor")))); }
    if (fn == "len") { need(1, 1); return a[0].is_string() ? Json(a[0].as_str().size()) : Json(a[0].size()); }
    if (fn == "str") { need(1, 1); return Json(to_text(a[0])); }
    if (fn == "int") { need(1, 1); if (a[0].is_string()) return Json(static_cast<int64_t>(std::strtoll(a[0].as_str().c_str(), nullptr, 10))); return Json(static_cast<int64_t>(num_of(a[0], "int"))); }
    if (fn == "float") { need(1, 1); if (a[0].is_string()) return Json(std::strtod(a[0].as_str().c_str(), nullptr)); return Json(num_of(a[0], "float")); }
    if (fn == "has") { need(2, 2); return Json(a[0].is_object() && a[1].is_string() && a[0].has(a[1].as_str())); }
    if (fn == "lower") { need(1, 1); return Json(lower(a[0].as_str())); }
    if (fn == "seen") { need(1, 1); return Json(ctx.seen && ctx.seen(a[0].as_str())); }
    if (fn == "rand" || fn == "chance" || fn == "choice") {
        if (!ctx.rng) bad("random numbers are unavailable here");
        uint64_t r = rng_next(*ctx.rng);
        if (fn == "chance") { need(1, 1); return Json(static_cast<double>(r >> 11) / 9007199254740992.0 < num_of(a[0], "chance")); }
        if (fn == "choice") { need(1, 1); if (!a[0].is_array() || a[0].size() == 0) bad("choice() needs a non-empty list"); return a[0].at(r % a[0].size()); }
        need(1, 2);
        int64_t lo = a.size() == 2 ? int_of(a[0]) : 0, hi = a.size() == 2 ? int_of(a[1]) : int_of(a[0]) - 1;
        if (hi < lo) bad("rand() range is empty");
        return Json(lo + static_cast<int64_t>(r % static_cast<uint64_t>(hi - lo + 1)));
    }
    bad("unknown function " + fn + "()");
}
} // namespace

Expr parse_expr(std::string_view text) {
    Parser p(text);
    return p.parse();
}

uint64_t rng_next(uint64_t& s) {
    if (s == 0) s = 0x9E3779B97F4A7C15ull;
    s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
    return s * 2685821657736338717ull;
}

bool truthy(const Json& v) {
    switch (v.type()) {
        case Json::Type::Null: return false;
        case Json::Type::Bool: return v.as_bool();
        case Json::Type::Int: case Json::Type::Real: return v.as_num() != 0;
        case Json::Type::String: return !v.as_str().empty();
        default: return v.size() > 0;
    }
}

Json eval(const Expr& e, EvalContext& ctx) {
    switch (e->kind) {
        case K::Num: case K::Str: case K::Bool: return e->value;
        case K::Null: return Json();
        case K::Var: { auto p = ctx.vars ? ctx.vars->find(e->name) : nullptr; return p ? *p : Json(); }
        case K::Member: { Json base = eval(e->kids[0], ctx); return base[e->name]; }
        case K::Index: {
            Json base = eval(e->kids[0], ctx), idx = eval(e->kids[1], ctx);
            if (base.is_array() && idx.is_number()) { int64_t i = idx.as_int(); if (i < 0) i += static_cast<int64_t>(base.size()); return base.at(static_cast<size_t>(i < 0 ? base.size() : i)); }
            if (base.is_object() && idx.is_string()) return base[idx.as_str()];
            return Json();
        }
        case K::List: { Json a = Json::array(); for (auto& k : e->kids) a.push(eval(k, ctx)); return a; }
        case K::Call: { std::vector<Json> args; for (auto& k : e->kids) args.push_back(eval(k, ctx)); return call(e->name, args, ctx); }
        case K::Unary: {
            Json v = eval(e->kids[0], ctx);
            if (e->name == "not") return Json(!truthy(v));
            return v.is_int() ? Json(-v.as_int()) : Json(-num_of(v, "negation"));
        }
        case K::Binary: {
            const std::string& o = e->name;
            if (o == "and") { Json l = eval(e->kids[0], ctx); return truthy(l) ? eval(e->kids[1], ctx) : l; }
            if (o == "or") { Json l = eval(e->kids[0], ctx); return truthy(l) ? l : eval(e->kids[1], ctx); }
            return apply_op(o, eval(e->kids[0], ctx), eval(e->kids[1], ctx));
        }
    }
    return Json();
}

Json apply_op(const std::string& o, const Json& l, const Json& r) {
            if (o == "==") return Json(l == r);
            if (o == "!=") return Json(l != r);
            if (o == "<") return Json(compare(l, r) < 0);
            if (o == "<=") return Json(compare(l, r) <= 0);
            if (o == ">") return Json(compare(l, r) > 0);
            if (o == ">=") return Json(compare(l, r) >= 0);
            if (o == "in") {
                if (r.is_string()) return Json(r.as_str().find(to_text(l)) != std::string::npos);
                if (r.is_array()) { for (auto& x : r.items()) if (x == l) return Json(true); return Json(false); }
                if (r.is_object()) return Json(l.is_string() && r.has(l.as_str()));
                return Json(false);
            }
            if (o == "+") {
                if (l.is_string() || r.is_string()) return Json(to_text(l) + to_text(r));
                if (l.is_array() && r.is_array()) { Json a = l; for (auto& x : r.items()) a.push(x); return a; }
                if (both_int(l, r)) return Json(int_of(l) + int_of(r));
                return Json(num_of(l, "+") + num_of(r, "+"));
            }
            if (o == "-") { if (both_int(l, r)) return Json(int_of(l) - int_of(r)); return Json(num_of(l, "-") - num_of(r, "-")); }
            if (o == "*") { if (both_int(l, r)) return Json(int_of(l) * int_of(r)); return Json(num_of(l, "*") * num_of(r, "*")); }
            if (o == "/") { double d = num_of(r, "/"); if (d == 0) bad("division by zero"); return Json(num_of(l, "/") / d); }
            if (o == "//") { double d = num_of(r, "//"); if (d == 0) bad("division by zero"); return Json(static_cast<int64_t>(std::floor(num_of(l, "//") / d))); }
            if (o == "%") { if (!both_int(l, r) || int_of(r) == 0) bad("% needs non-zero integers"); int64_t m = int_of(l) % int_of(r); if (m != 0 && ((m < 0) != (int_of(r) < 0))) m += int_of(r); return Json(m); }
    bad("unknown operator " + o);
}

bool is_lvalue(const Expr& e) {
    return e->kind == K::Var || ((e->kind == K::Member || e->kind == K::Index) && is_lvalue(e->kids[0]));
}

static Json* slot(const Expr& e, EvalContext& ctx) {
    if (e->kind == K::Var) {
        if (!ctx.vars) bad("no variable store");
        if (auto p = ctx.vars->find(e->name)) return p;
        return &ctx.vars->set(e->name, Json());
    }
    Json* base = slot(e->kids[0], ctx);
    if (e->kind == K::Member) {
        if (base->is_null()) *base = Json::object();
        if (!base->is_object()) bad("cannot set ." + e->name + " on a non-object");
        if (auto p = base->find(e->name)) return p;
        return &base->set(e->name, Json());
    }
    Json idx = eval(e->kids[1], ctx);
    if (base->is_null()) *base = idx.is_string() ? Json::object() : Json::array();
    if (base->is_array() && idx.is_number()) {
        auto& a = base->items_mut();
        int64_t i = idx.as_int();
        if (i < 0 || i > 100000) bad("list index out of range");
        while (a.size() <= static_cast<size_t>(i)) a.push_back(Json());
        return &a[static_cast<size_t>(i)];
    }
    if (base->is_object() && idx.is_string()) {
        if (auto p = base->find(idx.as_str())) return p;
        return &base->set(idx.as_str(), Json());
    }
    bad("cannot index here");
}

void assign(const Expr& lhs, EvalContext& ctx, Json value) {
    if (!is_lvalue(lhs)) bad("cannot assign to this expression");
    *slot(lhs, ctx) = std::move(value);
}

void collect_roots(const Expr& e, std::vector<std::string>& out) {
    if (e->kind == K::Var) out.push_back(e->name);
    for (auto& k : e->kids) collect_roots(k, out);
}

} // namespace sf
