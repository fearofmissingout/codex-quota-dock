#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cqd {

class JsonError : public std::runtime_error {
public:
    explicit JsonError(const std::string& message) : std::runtime_error(message) {}
};

class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    JsonValue();
    JsonValue(std::nullptr_t);
    JsonValue(bool value);
    JsonValue(double value);
    JsonValue(std::string value);
    JsonValue(Array value);
    JsonValue(Object value);

    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0) const;
    std::string asString(std::string fallback = {}) const;
    const Array& asArray() const;
    const Object& asObject() const;
    Array& asArray();
    Object& asObject();

    const JsonValue* get(std::string_view key) const;
    JsonValue* get(std::string_view key);

    static JsonValue parse(std::string_view text);
    std::string stringify(int indent = 2) const;

private:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
    Storage storage_;
};

std::string jsonEscape(std::string_view text);
std::string jsonString(std::string_view text);

} // namespace cqd
