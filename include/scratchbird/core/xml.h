/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

namespace scratchbird::core
{
    /**
     * XMLAttribute - Represents an XML attribute (name="value")
     */
    struct XMLAttribute {
        std::string name;
        std::string value;

        XMLAttribute(const std::string& n, const std::string& v) : name(n), value(v) {}
    };

    /**
     * XMLNode - Represents an XML element
     */
    class XMLNode {
    public:
        std::string name;
        std::unordered_map<std::string, std::string> attributes;
        std::string text;
        std::vector<std::shared_ptr<XMLNode>> children;

        XMLNode(const std::string& name);

        // Add attribute
        void setAttribute(const std::string& name, const std::string& value);
        auto getAttribute(const std::string& name) const -> std::optional<std::string>;

        // Add child
        void addChild(std::shared_ptr<XMLNode> child);

        // Find children by tag name
        std::vector<std::shared_ptr<XMLNode>> findChildren(const std::string& tag) const;

        // XPath-like queries
        // Examples: "book", "book/title", "//title", "book[@id='1']"
        std::vector<std::shared_ptr<XMLNode>> query(const std::string& path) const;

        // Convert to XML string
        std::string toXML(int indent = 0) const;
    };

    /**
     * XML - XML parsing and utilities
     */
    class XML {
    public:
        // Parse XML string to XMLNode tree
        static auto parse(const std::string& xml) -> std::optional<std::shared_ptr<XMLNode>>;

        // Validate XML string
        static bool validate(const std::string& xml);

        // Pretty print XML
        static std::string format(const std::string& xml);

        // Allow XMLNode to access private helper methods
        friend class XMLNode;

    private:
        // Parser helpers
        static auto parseElement(const std::string& xml, size_t& pos) -> std::optional<std::shared_ptr<XMLNode>>;
        static auto parseAttributes(const std::string& xml, size_t& pos) -> std::unordered_map<std::string, std::string>;
        static auto parseAttributeValue(const std::string& xml, size_t& pos) -> std::optional<std::string>;
        static auto parseName(const std::string& xml, size_t& pos) -> std::optional<std::string>;
        static auto parseText(const std::string& xml, size_t& pos) -> std::string;
        static void skipWhitespace(const std::string& xml, size_t& pos);
        static std::string decodeEntities(const std::string& str);
        static std::string encodeEntities(const std::string& str);

        // XPath helpers
        static std::vector<std::shared_ptr<XMLNode>> queryPath(
            const std::shared_ptr<XMLNode>& root,
            const std::vector<std::string>& path_parts,
            size_t index);
    };

} // namespace scratchbird::core
