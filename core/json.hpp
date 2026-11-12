// Small ordered JSON value, parser and writer. No third-party code.
#pragma once
#include "common.hpp"
#include <utility>
#include <variant>

namespace sf {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>; // insertion ordered => stable saves/diffs
    enum class Type { Null, Bool, Int, Real, String, Array, Object };

    Json() : v_(nullptr) {}
    Json(std::nullptr_t) : v_(nullptr) {}
    Json(bool b) : v_(b) {}
    Json(int i) : v_(static_cast<int64_t>(i)) {}
    Json(int64_t i) : v_(i) {}
    Json(size_t i) : v_(static_cast<int64_t>(i)) {}
    Json(double d) : v_(d) {}
    Json(const char* s) : v_(std::string(s)) {}
    Json(std::string s) : v_(std::move(s)) {}
    Json(std::string_view s) : v_(std::string(s)) {}
    Json(Array a) : v_(std::move(a)) {}
    Json(Object o) : v_(std::move(o)) {}

    static Json array() { return Json(Array{}); }
    static Json object() { return Json(Object{}); }

    Type type() const { return static_cast<Type>(v_.index()); }
    bool is_null() const { return type() == Type::Null; }
    bool is_bool() const { return type() == Type::Bool; }
    bool is_int() const { return type() == Type::Int; }
    bool is_number() const { return type() == Type::Int || type() == Type::Real; }
    bool is_string() const { return type() == Type::String; }
    bool is_array() const { return type() == Type::Array; }
    bool is_object() const { return type() == Type::Object; }

    bool as_bool(bool def = false) const;
    int64_t as_int(int64_t def = 0) const;
    double as_num(double def = 0) const;
    const std::string& as_str() const; // empty string if not a string
    std::string str_or(std::string def) const { return is_string() ? as_str() : def; }

    const Array& items() const;   // empty if not an array
    Array& items_mut();           // converts to array if null
    const Object& members() const; // empty if not an object
    size_t size() const;

    const Json* find(std::string_view key) const;
    Json* find(std::string_view key);
    const Json& operator[](std::string_view key) const; // null if missing
    const Json& at(size_t i) const;                      // null if out of range
    bool has(std::string_view key) const { return find(key) != nullptr; }
    Json& set(std::string key, Json v);                  // converts null to object
    Json& push(Json v);                                  // converts null to array
    bool erase(std::string_view key);

    static Json parse(std::string_view text);
    std::string dump(int indent = -1) const;

    bool operator==(const Json& o) const;
    bool operator!=(const Json& o) const { return !(*this == o); }

private:
    std::variant<std::nullptr_t, bool, int64_t, double, std::string, Array, Object> v_;
    void write(std::string& out, int indent, int depth) const;
};

} // namespace sf
