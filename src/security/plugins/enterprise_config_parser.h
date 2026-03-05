/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include <cctype>
#include <cstdint>
#include <map>
#include <string>

#include "scratchbird/security/auth_plugin_abi_v1.h"

namespace scratchbird {
namespace security {
namespace plugins {
namespace enterprise {

inline std::string trimAscii(std::string value) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

struct JsonParseState {
    const std::string* text = nullptr;
    std::size_t pos = 0;
    std::string error;
};

inline void skipWs(JsonParseState& s) {
    const std::string& text = *s.text;
    while (s.pos < text.size() && std::isspace(static_cast<unsigned char>(text[s.pos])) != 0) {
        ++s.pos;
    }
}

inline void setError(JsonParseState& s, const std::string& message) {
    if (s.error.empty()) {
        s.error = message + " at byte " + std::to_string(s.pos);
    }
}

inline bool consumeChar(JsonParseState& s, char ch) {
    skipWs(s);
    const std::string& text = *s.text;
    if (s.pos < text.size() && text[s.pos] == ch) {
        ++s.pos;
        return true;
    }
    return false;
}

inline bool expectChar(JsonParseState& s, char ch, const std::string& message) {
    if (consumeChar(s, ch)) {
        return true;
    }
    setError(s, message);
    return false;
}

inline bool parseHex4(JsonParseState& s, uint16_t& out_value) {
    const std::string& text = *s.text;
    if (s.pos + 4 > text.size()) {
        setError(s, "Invalid unicode escape sequence");
        return false;
    }

    out_value = 0;
    for (int i = 0; i < 4; ++i) {
        const char c = text[s.pos++];
        out_value = static_cast<uint16_t>(out_value << 4);
        if (c >= '0' && c <= '9') {
            out_value = static_cast<uint16_t>(out_value + (c - '0'));
        } else if (c >= 'a' && c <= 'f') {
            out_value = static_cast<uint16_t>(out_value + (c - 'a' + 10));
        } else if (c >= 'A' && c <= 'F') {
            out_value = static_cast<uint16_t>(out_value + (c - 'A' + 10));
        } else {
            setError(s, "Invalid unicode escape sequence");
            return false;
        }
    }

    return true;
}

inline bool parseJsonString(JsonParseState& s, std::string& out) {
    skipWs(s);
    const std::string& text = *s.text;
    if (s.pos >= text.size() || text[s.pos] != '"') {
        setError(s, "Expected JSON string");
        return false;
    }
    ++s.pos;

    out.clear();
    while (s.pos < text.size()) {
        const char ch = text[s.pos++];
        if (ch == '"') {
            return true;
        }
        if (static_cast<unsigned char>(ch) < 0x20) {
            setError(s, "Control character is not allowed in JSON string");
            return false;
        }
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }

        if (s.pos >= text.size()) {
            setError(s, "Invalid JSON escape sequence");
            return false;
        }

        const char esc = text[s.pos++];
        switch (esc) {
            case '"':
                out.push_back('"');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '/':
                out.push_back('/');
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'u': {
                uint16_t codepoint = 0;
                if (!parseHex4(s, codepoint)) {
                    return false;
                }
                if (codepoint <= 0x7F) {
                    out.push_back(static_cast<char>(codepoint));
                } else {
                    out.push_back('?');
                }
                break;
            }
            default:
                setError(s, "Invalid JSON escape sequence");
                return false;
        }
    }

    setError(s, "Unterminated JSON string");
    return false;
}

inline bool parseJsonLiteral(JsonParseState& s, const char* literal) {
    const std::string& text = *s.text;
    for (const char* p = literal; *p != '\0'; ++p) {
        if (s.pos >= text.size() || text[s.pos] != *p) {
            return false;
        }
        ++s.pos;
    }
    return true;
}

inline bool parseJsonNumber(JsonParseState& s, std::string& out) {
    skipWs(s);
    const std::string& text = *s.text;
    const std::size_t start = s.pos;

    if (s.pos < text.size() && text[s.pos] == '-') {
        ++s.pos;
    }

    if (s.pos >= text.size()) {
        setError(s, "Invalid JSON number");
        return false;
    }

    if (text[s.pos] == '0') {
        ++s.pos;
    } else if (text[s.pos] >= '1' && text[s.pos] <= '9') {
        ++s.pos;
        while (s.pos < text.size() && text[s.pos] >= '0' && text[s.pos] <= '9') {
            ++s.pos;
        }
    } else {
        setError(s, "Invalid JSON number");
        return false;
    }

    if (s.pos < text.size() && text[s.pos] == '.') {
        ++s.pos;
        if (s.pos >= text.size() || text[s.pos] < '0' || text[s.pos] > '9') {
            setError(s, "Invalid JSON number");
            return false;
        }
        while (s.pos < text.size() && text[s.pos] >= '0' && text[s.pos] <= '9') {
            ++s.pos;
        }
    }

    if (s.pos < text.size() && (text[s.pos] == 'e' || text[s.pos] == 'E')) {
        ++s.pos;
        if (s.pos < text.size() && (text[s.pos] == '+' || text[s.pos] == '-')) {
            ++s.pos;
        }
        if (s.pos >= text.size() || text[s.pos] < '0' || text[s.pos] > '9') {
            setError(s, "Invalid JSON number");
            return false;
        }
        while (s.pos < text.size() && text[s.pos] >= '0' && text[s.pos] <= '9') {
            ++s.pos;
        }
    }

    out = text.substr(start, s.pos - start);
    return true;
}

inline bool parseScalarValue(JsonParseState& s, std::string& out) {
    skipWs(s);
    const std::string& text = *s.text;
    if (s.pos >= text.size()) {
        setError(s, "Unexpected end of JSON input");
        return false;
    }

    const char ch = text[s.pos];
    if (ch == '"') {
        return parseJsonString(s, out);
    }

    if (ch == 't') {
        if (!parseJsonLiteral(s, "true")) {
            setError(s, "Invalid JSON literal");
            return false;
        }
        out = "true";
        return true;
    }

    if (ch == 'f') {
        if (!parseJsonLiteral(s, "false")) {
            setError(s, "Invalid JSON literal");
            return false;
        }
        out = "false";
        return true;
    }

    if (ch == 'n') {
        setError(s, "null values are not supported in plugin configs");
        return false;
    }

    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        return parseJsonNumber(s, out);
    }

    setError(s, "Config values must be scalar or array of scalars");
    return false;
}

inline bool parseArrayValue(JsonParseState& s, std::string& out) {
    if (!expectChar(s, '[', "Expected '[' to begin JSON array")) {
        return false;
    }

    std::string joined;
    skipWs(s);
    if (consumeChar(s, ']')) {
        out.clear();
        return true;
    }

    while (true) {
        std::string item;
        if (!parseScalarValue(s, item)) {
            return false;
        }
        if (!item.empty()) {
            if (!joined.empty()) {
                joined.push_back(',');
            }
            joined += item;
        }

        skipWs(s);
        if (consumeChar(s, ',')) {
            continue;
        }
        if (consumeChar(s, ']')) {
            break;
        }

        setError(s, "Expected ',' or ']' in JSON array");
        return false;
    }

    out = joined;
    return true;
}

inline bool parseValue(JsonParseState& s, std::string& out) {
    skipWs(s);
    const std::string& text = *s.text;
    if (s.pos >= text.size()) {
        setError(s, "Unexpected end of JSON input");
        return false;
    }

    if (text[s.pos] == '[') {
        return parseArrayValue(s, out);
    }
    if (text[s.pos] == '{') {
        setError(s, "Config object values must not be nested objects");
        return false;
    }

    return parseScalarValue(s, out);
}

inline bool parseObject(JsonParseState& s, std::map<std::string, std::string>& out) {
    if (!expectChar(s, '{', "Config payload must be a JSON object")) {
        return false;
    }

    skipWs(s);
    if (consumeChar(s, '}')) {
        return true;
    }

    while (true) {
        std::string key;
        if (!parseJsonString(s, key)) {
            return false;
        }
        key = trimAscii(std::move(key));
        if (key.empty()) {
            setError(s, "Config object contains an empty key");
            return false;
        }

        if (!expectChar(s, ':', "Expected ':' after config key")) {
            return false;
        }

        std::string value;
        if (!parseValue(s, value)) {
            return false;
        }

        out[key] = value;

        skipWs(s);
        if (consumeChar(s, ',')) {
            continue;
        }
        if (consumeChar(s, '}')) {
            break;
        }

        setError(s, "Expected ',' or '}' in config object");
        return false;
    }

    return true;
}

inline std::map<std::string, std::string> parseFlatConfig(sb_auth_slice_t config_json,
                                                           std::string* error_out = nullptr) {
    std::map<std::string, std::string> out;

    if (!config_json.ptr || config_json.len == 0) {
        return out;
    }

    std::string text(reinterpret_cast<const char*>(config_json.ptr),
                     reinterpret_cast<const char*>(config_json.ptr) + config_json.len);
    text = trimAscii(std::move(text));
    if (text.empty()) {
        return out;
    }

    JsonParseState state{&text, 0, ""};
    if (!parseObject(state, out)) {
        if (error_out) {
            *error_out = state.error.empty() ? "Invalid JSON config payload" : state.error;
        }
        return std::map<std::string, std::string>{};
    }

    skipWs(state);
    if (state.pos != text.size()) {
        if (error_out) {
            *error_out = "Unexpected trailing content in JSON config at byte " +
                         std::to_string(state.pos);
        }
        return std::map<std::string, std::string>{};
    }

    if (out.empty()) {
        if (error_out) {
            *error_out = "Config object did not contain any keys";
        }
        return std::map<std::string, std::string>{};
    }

    return out;
}

}  // namespace enterprise
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
