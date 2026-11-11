// Minimal test harness (no external dependencies).
#pragma once
#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace sft {
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Reg { Reg(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
template <class T> std::string show(const T& v) { std::ostringstream o; o << v; return o.str(); }
} // namespace sft

#define TEST(name) static void test_##name(); static sft::Reg reg_##name(#name, test_##name); static void test_##name()
#define CHECK(cond) do { if (!(cond)) { ++sft::failures(); std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; } } while (0)
#define CHECK_EQ(a, b) do { auto&& _a = (a); auto&& _b = (b); if (!(_a == _b)) { ++sft::failures(); std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b "\n    left:  " << sft::show(_a) << "\n    right: " << sft::show(_b) << "\n"; } } while (0)
#define CHECK_NEAR(a, b, eps) do { double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (eps))) { ++sft::failures(); std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #a " ~ " #b " (" << _a << " vs " << _b << ")\n"; } } while (0)
#define CHECK_THROWS(expr) do { bool _t = false; try { (void)(expr); } catch (const std::exception&) { _t = true; } if (!_t) { ++sft::failures(); std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": expected exception from " #expr "\n"; } } while (0)
