// Copyright (c) 2024-2026 Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Drop-in compatibility layer: json_spirit API backed by nlohmann/json.
// Provides the same types (Value, Object, Array, Pair) and functions
// (find_value, write_string, read_string) in the json_spirit namespace,
// so existing RPC code compiles without changes.

#ifndef JSON_COMPAT_H
#define JSON_COMPAT_H

#include "nlohmann_json.hpp"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace json_spirit {

// Use ordered_json to preserve insertion order (matches json_spirit's vector<Pair> behavior)
typedef nlohmann::ordered_json json_internal;

// ---------- Type enum (matches json_spirit exactly) ----------

enum Value_type {
    obj_type   = 0,
    array_type = 1,
    str_type   = 2,
    bool_type  = 3,
    int_type   = 4,
    real_type  = 5,
    null_type  = 6
};

inline const char* ValueTypeName(Value_type type)
{
    switch (type)
    {
    case obj_type:   return "obj";
    case array_type: return "array";
    case str_type:   return "str";
    case bool_type:  return "bool";
    case int_type:   return "int";
    case real_type:  return "real";
    case null_type:  return "null";
    }
    return "unknown";
}

// ---------- Forward declarations ----------

class Value;
struct Pair;

// ---------- Value ----------

class Value {
    json_internal j_;

    friend std::string write_string(const Value& val, bool pretty);
    friend bool read_string(const std::string& s, Value& val);
    friend bool read(const std::string& s, Value& val);

public:
    // Default: null
    Value() : j_(nullptr) {}

    // Primitives
    Value(const char* s)        : j_(std::string(s)) {}
    Value(const std::string& s) : j_(s) {}
    Value(bool b)               : j_(b) {}
    Value(int i)                : j_(static_cast<int64_t>(i)) {}
    Value(unsigned int u)       : j_(static_cast<uint64_t>(u)) {}
    Value(int64_t i)            : j_(i) {}
    Value(uint64_t u)           : j_(u) {}
    Value(double d)             : j_(d) {}

    // Compound types (defined after Pair/Object/Array)
    inline Value(const std::vector<Pair>& obj);
    inline Value(const std::vector<Value>& arr);

    // Type introspection
    Value_type type() const {
        if (j_.is_null())            return null_type;
        if (j_.is_object())          return obj_type;
        if (j_.is_array())           return array_type;
        if (j_.is_string())          return str_type;
        if (j_.is_boolean())         return bool_type;
        if (j_.is_number_integer())  return int_type;
        if (j_.is_number_float())    return real_type;
        return null_type;
    }

    bool is_null()   const { return j_.is_null(); }
    bool is_uint64() const { return j_.is_number_unsigned(); }

    // Accessors (throw on type mismatch, matching json_spirit semantics)
    const std::string& get_str() const {
        if (!j_.is_string())
            throw std::runtime_error("value type is not str");
        return j_.get_ref<const std::string&>();
    }

    bool get_bool() const {
        if (!j_.is_boolean())
            throw std::runtime_error("value type is not bool");
        return j_.get<bool>();
    }

    int get_int() const {
        if (!j_.is_number_integer())
            throw std::runtime_error("value type is not int");
        return j_.get<int>();
    }

    int64_t get_int64() const {
        if (!j_.is_number_integer())
            throw std::runtime_error("value type is not int");
        return j_.get<int64_t>();
    }

    uint64_t get_uint64() const {
        if (!j_.is_number_integer())
            throw std::runtime_error("value type is not int");
        return j_.get<uint64_t>();
    }

    double get_real() const {
        // json_spirit converts ints to double in get_real()
        if (!j_.is_number())
            throw std::runtime_error("value type is not numeric");
        return j_.get<double>();
    }

    // Compound accessors (defined after Pair/Object/Array)
    inline std::vector<Pair> get_obj() const;
    inline std::vector<Value> get_array() const;

    // Map-based object accessor for mValue compatibility
    inline std::map<std::string, Value> get_map_obj() const;

    // Template accessor (used by ConvertTo<T>)
    template<typename T> T get_value() const;

    // Static null constant (matches json_spirit::Value::null)
    static const Value null;

    bool operator==(const Value& o) const { return j_ == o.j_; }
    bool operator!=(const Value& o) const { return j_ != o.j_; }
};

// Define the static member
inline const Value Value::null{};

// ---------- Pair ----------

struct Pair {
    std::string name_;
    Value       value_;

    Pair() {}
    Pair(const std::string& name, const Value& value)
        : name_(name), value_(value) {}

    bool operator==(const Pair& o) const { return name_ == o.name_; }
};

// ---------- Object / Array typedefs ----------

typedef std::vector<Pair>  Object;
typedef std::vector<Value> Array;

// ---------- Map-based aliases (mValue/mObject for messagemodel.cpp compat) ----------

typedef Value mValue;
typedef std::map<std::string, Value> mObject;

// ---------- Value compound constructors ----------

inline Value::Value(const std::vector<Pair>& obj) {
    j_ = json_internal::object();
    for (const auto& p : obj)
        j_[p.name_] = p.value_.j_;
}

inline Value::Value(const std::vector<Value>& arr) {
    j_ = json_internal::array();
    for (const auto& v : arr)
        j_.push_back(v.j_);
}

// ---------- Value compound accessors ----------

inline std::vector<Pair> Value::get_obj() const {
    if (!j_.is_object())
        throw std::runtime_error("value type is not obj");
    std::vector<Pair> result;
    result.reserve(j_.size());
    for (auto it = j_.begin(); it != j_.end(); ++it) {
        Value v;
        v.j_ = it.value();
        result.emplace_back(it.key(), v);
    }
    return result;
}

inline std::vector<Value> Value::get_array() const {
    if (!j_.is_array())
        throw std::runtime_error("value type is not array");
    std::vector<Value> result;
    result.reserve(j_.size());
    for (const auto& elem : j_) {
        Value v;
        v.j_ = elem;
        result.push_back(v);
    }
    return result;
}

inline std::map<std::string, Value> Value::get_map_obj() const {
    if (!j_.is_object())
        throw std::runtime_error("value type is not obj");
    std::map<std::string, Value> result;
    for (auto it = j_.begin(); it != j_.end(); ++it) {
        Value v;
        v.j_ = it.value();
        result[it.key()] = v;
    }
    return result;
}

// ---------- Template get_value specializations ----------

template<> inline std::string Value::get_value<std::string>() const { return get_str(); }
template<> inline bool        Value::get_value<bool>()        const { return get_bool(); }
template<> inline int         Value::get_value<int>()         const { return get_int(); }
template<> inline int64_t     Value::get_value<int64_t>()     const { return get_int64(); }
template<> inline uint64_t    Value::get_value<uint64_t>()    const { return get_uint64(); }
template<> inline double      Value::get_value<double>()      const { return get_real(); }
template<> inline Object      Value::get_value<Object>()      const { return get_obj(); }
template<> inline Array       Value::get_value<Array>()       const { return get_array(); }

// ---------- Utility functions ----------

inline const Value& find_value(const Object& obj, const std::string& name) {
    for (const auto& p : obj)
        if (p.name_ == name)
            return p.value_;
    static const Value null_value;
    return null_value;
}

inline std::string write_string(const Value& val, bool pretty = false) {
    return val.j_.dump(pretty ? 4 : -1);
}

inline bool read_string(const std::string& s, Value& val) {
    try {
        val.j_ = json_internal::parse(s);
        return true;
    } catch (const nlohmann::detail::parse_error&) {
        return false;
    }
}

// Alias matching json_spirit::read() signature (used by messagemodel.cpp)
inline bool read(const std::string& s, Value& val) {
    return read_string(s, val);
}

// Stream-based reader (used by test harness read_json)
template<typename Istream>
inline bool read_stream(Istream& is, Value& val) {
    std::string s((std::istreambuf_iterator<char>(is)),
                   std::istreambuf_iterator<char>());
    return read_string(s, val);
}

} // namespace json_spirit

#endif // JSON_COMPAT_H
