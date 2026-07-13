#include "json.h"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace cqd {
namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    JsonValue parse() {
        skipUtf8Bom();
        skipSpace();
        JsonValue value = parseValue();
        skipSpace();
        if (pos_ != text_.size()) {
            fail("unexpected trailing content");
        }
        return value;
    }

private:
    JsonValue parseValue() {
        skipSpace();
        if (pos_ >= text_.size()) {
            fail("unexpected end of json");
        }
        char c = text_[pos_];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return JsonValue(parseString());
        if (c == 't') return parseLiteral("true", JsonValue(true));
        if (c == 'f') return parseLiteral("false", JsonValue(false));
        if (c == 'n') return parseLiteral("null", JsonValue(nullptr));
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        fail("unexpected token");
    }

    JsonValue parseObject() {
        consume('{');
        JsonValue::Object object;
        skipSpace();
        if (match('}')) {
            return JsonValue(std::move(object));
        }
        while (true) {
            skipSpace();
            if (peek() != '"') {
                fail("object key must be a string");
            }
            std::string key = parseString();
            skipSpace();
            consume(':');
            object.emplace(std::move(key), parseValue());
            skipSpace();
            if (match('}')) break;
            consume(',');
        }
        return JsonValue(std::move(object));
    }

    JsonValue parseArray() {
        consume('[');
        JsonValue::Array array;
        skipSpace();
        if (match(']')) {
            return JsonValue(std::move(array));
        }
        while (true) {
            array.push_back(parseValue());
            skipSpace();
            if (match(']')) break;
            consume(',');
        }
        return JsonValue(std::move(array));
    }

    JsonValue parseNumber() {
        size_t start = pos_;
        if (match('-')) {}
        consumeDigits();
        if (match('.')) consumeDigits();
        if (match('e') || match('E')) {
            if (match('+') || match('-')) {}
            consumeDigits();
        }
        double value = 0;
        std::string number(text_.substr(start, pos_ - start));
        auto [ptr, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
        if (ec != std::errc() || ptr != number.data() + number.size()) {
            fail("invalid number");
        }
        return JsonValue(value);
    }

    JsonValue parseLiteral(std::string_view literal, JsonValue value) {
        if (text_.substr(pos_, literal.size()) != literal) {
            fail("invalid literal");
        }
        pos_ += literal.size();
        return value;
    }

    std::string parseString() {
        consume('"');
        std::string out;
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') {
                return out;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                fail("unterminated escape");
            }
            char e = text_[pos_++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
                appendUnicodeEscape(out);
                break;
            default:
                fail("invalid escape");
            }
        }
        fail("unterminated string");
    }

    void appendUnicodeEscape(std::string& out) {
        if (pos_ + 4 > text_.size()) {
            fail("short unicode escape");
        }
        uint32_t code = 0;
        for (int i = 0; i < 4; ++i) {
            char c = text_[pos_++];
            code <<= 4;
            if (c >= '0' && c <= '9') code += static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') code += static_cast<uint32_t>(10 + c - 'a');
            else if (c >= 'A' && c <= 'F') code += static_cast<uint32_t>(10 + c - 'A');
            else fail("invalid unicode escape");
        }
        if (code <= 0x7F) {
            out.push_back(static_cast<char>(code));
        } else if (code <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    void consumeDigits() {
        size_t start = pos_;
        while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
            ++pos_;
        }
        if (start == pos_) fail("expected digit");
    }

    void skipSpace() {
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') break;
            ++pos_;
        }
    }

    void skipUtf8Bom() {
        if (text_.size() >= 3 &&
            static_cast<unsigned char>(text_[0]) == 0xEF &&
            static_cast<unsigned char>(text_[1]) == 0xBB &&
            static_cast<unsigned char>(text_[2]) == 0xBF) {
            pos_ = 3;
        }
    }

    char peek() const {
        return pos_ < text_.size() ? text_[pos_] : '\0';
    }

    bool match(char expected) {
        if (pos_ < text_.size() && text_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    void consume(char expected) {
        if (!match(expected)) {
            fail(std::string("expected '") + expected + "'");
        }
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw JsonError(message + " at byte " + std::to_string(pos_));
    }

    std::string_view text_;
    size_t pos_ = 0;
};

void stringifyValue(const JsonValue& value, std::ostringstream& out, int indent, int depth) {
    if (value.isNull()) {
        out << "null";
    } else if (value.isBool()) {
        out << (value.asBool() ? "true" : "false");
    } else if (value.isNumber()) {
        double n = value.asNumber();
        if (std::floor(n) == n) {
            out << static_cast<int64_t>(n);
        } else {
            out << std::setprecision(12) << n;
        }
    } else if (value.isString()) {
        out << jsonString(value.asString());
    } else if (value.isArray()) {
        const auto& array = value.asArray();
        out << "[";
        for (size_t i = 0; i < array.size(); ++i) {
            if (i) out << ",";
            if (indent > 0) out << "\n" << std::string((depth + 1) * indent, ' ');
            stringifyValue(array[i], out, indent, depth + 1);
        }
        if (!array.empty() && indent > 0) out << "\n" << std::string(depth * indent, ' ');
        out << "]";
    } else {
        const auto& object = value.asObject();
        out << "{";
        bool first = true;
        for (const auto& [key, item] : object) {
            if (!first) out << ",";
            first = false;
            if (indent > 0) out << "\n" << std::string((depth + 1) * indent, ' ');
            out << jsonString(key) << ":";
            if (indent > 0) out << " ";
            stringifyValue(item, out, indent, depth + 1);
        }
        if (!object.empty() && indent > 0) out << "\n" << std::string(depth * indent, ' ');
        out << "}";
    }
}

} // namespace

JsonValue::JsonValue() : storage_(nullptr) {}
JsonValue::JsonValue(std::nullptr_t) : storage_(nullptr) {}
JsonValue::JsonValue(bool value) : storage_(value) {}
JsonValue::JsonValue(double value) : storage_(value) {}
JsonValue::JsonValue(std::string value) : storage_(std::move(value)) {}
JsonValue::JsonValue(Array value) : storage_(std::move(value)) {}
JsonValue::JsonValue(Object value) : storage_(std::move(value)) {}

bool JsonValue::isNull() const { return std::holds_alternative<std::nullptr_t>(storage_); }
bool JsonValue::isBool() const { return std::holds_alternative<bool>(storage_); }
bool JsonValue::isNumber() const { return std::holds_alternative<double>(storage_); }
bool JsonValue::isString() const { return std::holds_alternative<std::string>(storage_); }
bool JsonValue::isArray() const { return std::holds_alternative<Array>(storage_); }
bool JsonValue::isObject() const { return std::holds_alternative<Object>(storage_); }

bool JsonValue::asBool(bool fallback) const {
    return isBool() ? std::get<bool>(storage_) : fallback;
}

double JsonValue::asNumber(double fallback) const {
    return isNumber() ? std::get<double>(storage_) : fallback;
}

std::string JsonValue::asString(std::string fallback) const {
    return isString() ? std::get<std::string>(storage_) : fallback;
}

const JsonValue::Array& JsonValue::asArray() const {
    if (!isArray()) throw JsonError("json value is not an array");
    return std::get<Array>(storage_);
}

const JsonValue::Object& JsonValue::asObject() const {
    if (!isObject()) throw JsonError("json value is not an object");
    return std::get<Object>(storage_);
}

JsonValue::Array& JsonValue::asArray() {
    if (!isArray()) throw JsonError("json value is not an array");
    return std::get<Array>(storage_);
}

JsonValue::Object& JsonValue::asObject() {
    if (!isObject()) throw JsonError("json value is not an object");
    return std::get<Object>(storage_);
}

const JsonValue* JsonValue::get(std::string_view key) const {
    if (!isObject()) return nullptr;
    const auto& object = std::get<Object>(storage_);
    auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
}

JsonValue* JsonValue::get(std::string_view key) {
    if (!isObject()) return nullptr;
    auto& object = std::get<Object>(storage_);
    auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
}

JsonValue JsonValue::parse(std::string_view text) {
    return Parser(text).parse();
}

std::string JsonValue::stringify(int indent) const {
    std::ostringstream out;
    stringifyValue(*this, out, indent, 0);
    return out.str();
}

std::string jsonEscape(std::string_view text) {
    std::string out;
    for (unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                const char* hex = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(c >> 4) & 0xF]);
                out.push_back(hex[c & 0xF]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

std::string jsonString(std::string_view text) {
    return "\"" + jsonEscape(text) + "\"";
}

} // namespace cqd
