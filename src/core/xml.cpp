/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/xml.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace scratchbird::core
{
    // ===== XMLNode Implementation =====

    XMLNode::XMLNode(const std::string& n) : name(n) {}

    void XMLNode::setAttribute(const std::string& n, const std::string& v) {
        attributes[n] = v;
    }

    auto XMLNode::getAttribute(const std::string& n) const -> std::optional<std::string> {
        auto it = attributes.find(n);
        if (it == attributes.end()) return std::nullopt;
        return it->second;
    }

    void XMLNode::addChild(std::shared_ptr<XMLNode> child) {
        children.push_back(child);
    }

    std::vector<std::shared_ptr<XMLNode>> XMLNode::findChildren(const std::string& tag) const {
        std::vector<std::shared_ptr<XMLNode>> result;
        for (const auto& child : children) {
            if (child->name == tag) {
                result.push_back(child);
            }
        }
        return result;
    }

    std::vector<std::shared_ptr<XMLNode>> XMLNode::query(const std::string& path) const {
        // Simple XPath-like query: "book/title" or "//title"
        std::vector<std::string> parts;
        size_t start = 0;
        size_t pos = 0;

        while ((pos = path.find('/', start)) != std::string::npos) {
            if (pos > start) {
                parts.push_back(path.substr(start, pos - start));
            }
            start = pos + 1;
        }
        if (start < path.length()) {
            parts.push_back(path.substr(start));
        }

        auto root = std::make_shared<XMLNode>(*this);
        return XML::queryPath(root, parts, 0);
    }

    std::string XMLNode::toXML(int indent) const {
        std::string result;
        std::string ind(indent * 2, ' ');

        result += ind + "<" + name;
        for (const auto& [key, val] : attributes) {
            result += " " + key + "=\"" + XML::encodeEntities(val) + "\"";
        }

        if (children.empty() && text.empty()) {
            result += "/>\n";
        } else {
            result += ">";
            if (!text.empty()) {
                result += XML::encodeEntities(text);
            }
            if (!children.empty()) {
                result += "\n";
                for (const auto& child : children) {
                    result += child->toXML(indent + 1);
                }
                result += ind;
            }
            result += "</" + name + ">\n";
        }
        return result;
    }

    // ===== XML Implementation =====

    auto XML::parse(const std::string& xml) -> std::optional<std::shared_ptr<XMLNode>> {
        size_t pos = 0;
        skipWhitespace(xml, pos);

        // Skip XML declaration if present
        if (pos < xml.length() && xml.substr(pos, 5) == "<?xml") {
            while (pos < xml.length() && xml.substr(pos, 2) != "?>") {
                ++pos;
            }
            if (pos < xml.length()) pos += 2;
            skipWhitespace(xml, pos);
        }

        return parseElement(xml, pos);
    }

    bool XML::validate(const std::string& xml) {
        return parse(xml).has_value();
    }

    std::string XML::format(const std::string& xml) {
        auto root = parse(xml);
        if (!root) return "";
        return (*root)->toXML(0);
    }

    void XML::skipWhitespace(const std::string& xml, size_t& pos) {
        while (pos < xml.length() && std::isspace(xml[pos])) {
            ++pos;
        }
    }

    auto XML::parseName(const std::string& xml, size_t& pos) -> std::optional<std::string> {
        size_t start = pos;
        while (pos < xml.length() &&
               (std::isalnum(xml[pos]) || xml[pos] == '_' || xml[pos] == '-' || xml[pos] == ':')) {
            ++pos;
        }
        if (pos == start) return std::nullopt;
        return xml.substr(start, pos - start);
    }

    auto XML::parseAttributeValue(const std::string& xml, size_t& pos) -> std::optional<std::string> {
        if (pos >= xml.length()) return std::nullopt;
        char quote = xml[pos];
        if (quote != '"' && quote != '\'') return std::nullopt;
        ++pos;

        size_t start = pos;
        while (pos < xml.length() && xml[pos] != quote) {
            ++pos;
        }
        if (pos >= xml.length()) return std::nullopt;

        std::string value = xml.substr(start, pos - start);
        ++pos;
        return decodeEntities(value);
    }

    auto XML::parseAttributes(const std::string& xml, size_t& pos) -> std::unordered_map<std::string, std::string> {
        std::unordered_map<std::string, std::string> attrs;

        while (pos < xml.length()) {
            skipWhitespace(xml, pos);
            if (pos >= xml.length() || xml[pos] == '>' || xml[pos] == '/') {
                break;
            }

            auto name = parseName(xml, pos);
            if (!name) break;

            skipWhitespace(xml, pos);
            if (pos >= xml.length() || xml[pos] != '=') break;
            ++pos;

            skipWhitespace(xml, pos);
            auto value = parseAttributeValue(xml, pos);
            if (!value) break;

            attrs[*name] = *value;
        }

        return attrs;
    }

    auto XML::parseText(const std::string& xml, size_t& pos) -> std::string {
        size_t start = pos;
        while (pos < xml.length() && xml[pos] != '<') {
            ++pos;
        }
        std::string text = xml.substr(start, pos - start);

        // Trim whitespace
        size_t first = text.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = text.find_last_not_of(" \t\n\r");
        return decodeEntities(text.substr(first, last - first + 1));
    }

    auto XML::parseElement(const std::string& xml, size_t& pos) -> std::optional<std::shared_ptr<XMLNode>> {
        skipWhitespace(xml, pos);
        if (pos >= xml.length() || xml[pos] != '<') return std::nullopt;
        ++pos;

        // Parse tag name
        auto name = parseName(xml, pos);
        if (!name) return std::nullopt;

        auto node = std::make_shared<XMLNode>(*name);

        // Parse attributes
        node->attributes = parseAttributes(xml, pos);

        skipWhitespace(xml, pos);
        if (pos >= xml.length()) return std::nullopt;

        // Self-closing tag
        if (xml[pos] == '/') {
            ++pos;
            if (pos >= xml.length() || xml[pos] != '>') return std::nullopt;
            ++pos;
            return node;
        }

        // Close opening tag
        if (xml[pos] != '>') return std::nullopt;
        ++pos;

        // Parse content (text and children)
        bool found_closing_tag = false;
        while (pos < xml.length()) {
            skipWhitespace(xml, pos);
            if (pos >= xml.length()) break;

            // Check for closing tag
            if (xml[pos] == '<') {
                if (pos + 1 < xml.length() && xml[pos + 1] == '/') {
                    // Closing tag
                    pos += 2;
                    auto closeName = parseName(xml, pos);
                    if (!closeName || *closeName != *name) return std::nullopt;
                    skipWhitespace(xml, pos);
                    if (pos >= xml.length() || xml[pos] != '>') return std::nullopt;
                    ++pos;
                    found_closing_tag = true;
                    break;
                } else {
                    // Child element
                    auto child = parseElement(xml, pos);
                    if (!child) return std::nullopt;
                    node->addChild(*child);
                }
            } else {
                // Text content
                std::string text = parseText(xml, pos);
                if (!text.empty()) {
                    node->text = text;
                }
            }
        }

        // Must have found a closing tag (unless the element is empty with no content)
        if (!found_closing_tag) return std::nullopt;

        return node;
    }

    std::string XML::decodeEntities(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '&') {
                if (str.substr(i, 4) == "&lt;") {
                    result += '<';
                    i += 3;
                } else if (str.substr(i, 4) == "&gt;") {
                    result += '>';
                    i += 3;
                } else if (str.substr(i, 5) == "&amp;") {
                    result += '&';
                    i += 4;
                } else if (str.substr(i, 6) == "&quot;") {
                    result += '"';
                    i += 5;
                } else if (str.substr(i, 6) == "&apos;") {
                    result += '\'';
                    i += 5;
                } else {
                    result += str[i];
                }
            } else {
                result += str[i];
            }
        }
        return result;
    }

    std::string XML::encodeEntities(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '&': result += "&amp;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&apos;"; break;
                default: result += c;
            }
        }
        return result;
    }

    std::vector<std::shared_ptr<XMLNode>> XML::queryPath(
        const std::shared_ptr<XMLNode>& root,
        const std::vector<std::string>& path_parts,
        size_t index)
    {
        if (index >= path_parts.size()) {
            return {root};
        }

        const std::string& part = path_parts[index];
        std::vector<std::shared_ptr<XMLNode>> results;

        // Match current node
        if (root->name == part) {
            if (index == path_parts.size() - 1) {
                results.push_back(root);
            } else {
                // Continue with children
                for (const auto& child : root->children) {
                    auto child_results = queryPath(child, path_parts, index + 1);
                    results.insert(results.end(), child_results.begin(), child_results.end());
                }
            }
        }

        // Search children
        for (const auto& child : root->children) {
            auto child_results = queryPath(child, path_parts, index);
            results.insert(results.end(), child_results.begin(), child_results.end());
        }

        return results;
    }

} // namespace scratchbird::core
