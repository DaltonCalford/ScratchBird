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

/**
 * ScratchBird Parser v2.0 - Schema Path Syntax
 *
 * This module implements hierarchical schema path parsing for ScratchBird's
 * unique namespace navigation system. Schema paths allow users to reference
 * database objects using familiar directory-like syntax.
 *
 * Path Types:
 *   - UNQUALIFIED: `orders`           - Uses search path resolution
 *   - CURRENT:     `.orders`          - Explicit current schema
 *   - RELATIVE:    `.sub.orders`      - Relative from current schema
 *   - PARENT:      `..orders`         - Parent schema
 *   - ABSOLUTE:    `sys.catalog.tables` - Absolute path from root
 *   - NO_SEARCH:   `!:orders`         - Disable search path for unqualified names
 *
 * Grammar:
 *   <object_path> ::= [ <no_search_prefix> ] <schema_path>
 *   <no_search_prefix> ::= "!" ":"
 *   <schema_path> ::= <unqualified> | <current_path> | <parent_path> | <absolute_path>
 *   <unqualified> ::= <identifier>
 *   <current_path> ::= DOT <identifier> [ DOT <identifier> ]*
 *   <parent_path> ::= DOUBLE_DOT <identifier> [ DOT <identifier> ]*
 *   <absolute_path> ::= <identifier> DOT <identifier> [ DOT <identifier> ]*
 *
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md Section 6
 */

#include "scratchbird/parser/lexer_v2.h"
#include <vector>
#include <string>
#include <string_view>

namespace scratchbird::parser::v2 {

// Forward declarations
class ParserState;

/**
 * Schema path type - determines how the path is resolved
 */
enum class PathType : uint8_t {
    UNQUALIFIED,    // name (uses search path)
    CURRENT,        // .name or .path.name (relative to current schema)
    PARENT,         // ..name or ..path.name (relative to parent schema)
    ABSOLUTE        // schema.name or schema.path.name (from root)
};

/**
 * Schema path - represents a reference to a database object
 *
 * Examples:
 *   - PathType::UNQUALIFIED: components = ["orders"]
 *   - PathType::CURRENT:     components = ["orders"] (from ".orders")
 *   - PathType::CURRENT:     components = ["sub", "orders"] (from ".sub.orders")
 *   - PathType::PARENT:      components = ["orders"] (from "..orders")
 *   - PathType::ABSOLUTE:    components = ["sys", "catalog", "tables"]
 */
struct SchemaPath {
    PathType type = PathType::UNQUALIFIED;
    bool no_search_path = false;  // True when prefixed with !:
    std::vector<StringPool::StringId> components;
    SourceSpan span;  // Source location for error reporting

    // Convenience constructors
    SchemaPath() = default;
    SchemaPath(PathType t, std::vector<StringPool::StringId> comps, SourceSpan s = {},
               bool no_search = false)
        : type(t), no_search_path(no_search), components(std::move(comps)), span(s) {}

    // Query methods
    bool isQualified() const { return type != PathType::UNQUALIFIED; }
    bool isEmpty() const { return components.empty(); }
    size_t depth() const { return components.size(); }

    // Get the object name (last component)
    StringPool::StringId objectName() const {
        return components.empty() ? StringPool::INVALID_ID : components.back();
    }

    // Get the schema path (all but last component)
    std::vector<StringPool::StringId> schemaComponents() const {
        if (components.size() <= 1) return {};
        return std::vector<StringPool::StringId>(components.begin(), components.end() - 1);
    }

    // Comparison for testing
    bool operator==(const SchemaPath& other) const {
        return type == other.type &&
               no_search_path == other.no_search_path &&
               components == other.components;
    }
    bool operator!=(const SchemaPath& other) const {
        return !(*this == other);
    }
};

/**
 * Convert PathType to string for debugging
 */
const char* pathTypeToString(PathType type);

/**
 * Convert SchemaPath to human-readable string for debugging/errors
 *
 * @param path The schema path to convert
 * @param pool StringPool to look up component names
 * @return String representation like ".orders" or "sys.catalog.tables"
 */
std::string schemaPathToString(const SchemaPath& path, const StringPool& pool);

/**
 * Parse a schema path from the current position in the parser state
 *
 * This function handles all path types:
 *   - Starts with DOT: current schema path (.name or .path.name)
 *   - Starts with DOUBLE_DOT: parent schema path (..name)
 *   - Starts with IDENTIFIER:
 *     - If followed by DOT: absolute path (schema.name)
 *     - Otherwise: unqualified name
 *
 * @param state Parser state positioned at start of path
 * @return Parsed schema path
 */
SchemaPath parseSchemaPath(ParserState& state);

/**
 * Check if current token could start a schema path
 *
 * Returns true if current token is:
 *   - DOT (current schema: .name)
 *   - DOUBLE_DOT (parent schema: ..name)
 *   - IDENTIFIER (unqualified or absolute: name or schema.name)
 */
bool canStartSchemaPath(const ParserState& state);

/**
 * Table reference with optional alias
 *
 * Represents a table reference in FROM clause:
 *   - SELECT * FROM orders
 *   - SELECT * FROM orders AS o
 *   - SELECT * FROM .orders o
 *   - SELECT * FROM sys.catalog.tables t
 */
struct TableRef {
    SchemaPath path;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;
    SourceSpan span;

    TableRef() = default;
    TableRef(SchemaPath p, SourceSpan s = {})
        : path(std::move(p)), span(s) {}
    TableRef(SchemaPath p, StringPool::StringId a, SourceSpan s = {})
        : path(std::move(p)), alias(a), has_alias(true), span(s) {}
};

/**
 * Column reference with optional table qualifier
 *
 * Represents a column reference in expressions:
 *   - id                    (unqualified)
 *   - orders.id             (table qualified)
 *   - o.id                  (alias qualified)
 *   - .orders.id            (current schema table qualified)
 *   - sys.catalog.tables.name (fully qualified)
 */
struct ColumnRef {
    SchemaPath table_path;      // Empty for unqualified column references
    StringPool::StringId column_name = StringPool::INVALID_ID;
    bool has_table_qualifier = false;
    SourceSpan span;

    ColumnRef() = default;
    ColumnRef(StringPool::StringId col, SourceSpan s = {})
        : column_name(col), span(s) {}
    ColumnRef(SchemaPath table, StringPool::StringId col, SourceSpan s = {})
        : table_path(std::move(table)), column_name(col), has_table_qualifier(true), span(s) {}
};

/**
 * Parse a table reference (path with optional alias)
 *
 * Grammar:
 *   <table_ref> ::= <schema_path> [ [ AS ] <identifier> ]
 *
 * @param state Parser state
 * @return Parsed table reference
 */
TableRef parseTableRef(ParserState& state);

/**
 * Parse a column reference (possibly qualified with table path)
 *
 * This is tricky because we need lookahead to distinguish:
 *   - id                    (simple column)
 *   - table.id              (table.column)
 *   - schema.table.id       (schema.table.column)
 *   - .table.id             (current_schema.table.column)
 *
 * The last component is always the column name.
 *
 * @param state Parser state
 * @return Parsed column reference
 */
ColumnRef parseColumnRef(ParserState& state);

} // namespace scratchbird::parser::v2
