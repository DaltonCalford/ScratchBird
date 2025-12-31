/**
 * ScratchBird Parser v2.0 - Schema Path Implementation
 *
 * See: include/scratchbird/parser/schema_path_v2.h
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md Section 6
 */

#include "scratchbird/parser/schema_path_v2.h"
#include "scratchbird/parser/parser_state_v2.h"
#include <sstream>

namespace scratchbird::parser::v2 {

// =============================================================================
// Utility Functions
// =============================================================================

const char* pathTypeToString(PathType type) {
    switch (type) {
        case PathType::UNQUALIFIED: return "UNQUALIFIED";
        case PathType::CURRENT:     return "CURRENT";
        case PathType::PARENT:      return "PARENT";
        case PathType::ABSOLUTE:    return "ABSOLUTE";
    }
    return "UNKNOWN";
}

std::string schemaPathToString(const SchemaPath& path, const StringPool& pool) {
    std::ostringstream ss;

    if (path.no_search_path) {
        ss << "!:";
    }

    // Add prefix based on type
    switch (path.type) {
        case PathType::CURRENT:
            ss << ".";
            break;
        case PathType::PARENT:
            ss << "..";
            break;
        case PathType::UNQUALIFIED:
        case PathType::ABSOLUTE:
            // No prefix
            break;
    }

    // Add components
    bool first = true;
    for (auto id : path.components) {
        if (!first) {
            ss << ".";
        }
        first = false;
        ss << pool.get(id);
    }

    return ss.str();
}

bool canStartSchemaPath(const ParserState& state) {
    TokenType type = state.current().type;
    return type == TokenType::EXCLAIM_COLON ||
           type == TokenType::DOT ||
           type == TokenType::DOUBLE_DOT ||
           type == TokenType::IDENTIFIER;
}

// =============================================================================
// Schema Path Parsing
// =============================================================================

SchemaPath parseSchemaPath(ParserState& state) {
    SchemaPath path;
    SourceLocation start = state.currentLocation();
    bool no_search_path = false;

    if (state.check(TokenType::EXCLAIM_COLON)) {
        no_search_path = true;
        state.advance();
    }
    path.no_search_path = no_search_path;

    // Check what we're starting with
    if (state.check(TokenType::DOT)) {
        // Current schema path: .name or .path.name
        state.advance();  // consume DOT
        path.type = PathType::CURRENT;

        // Must have at least one identifier after DOT
        if (!state.isIdentifier()) {
            // Error: expected identifier after '.'
            return path;  // Return empty path
        }

        // Collect path components
        while (state.isIdentifier()) {
            path.components.push_back(state.currentStringId());
            state.advance();

            // Check for more path components
            if (state.check(TokenType::DOT)) {
                state.advance();  // consume DOT
                // Continue to next identifier
            } else {
                break;
            }
        }
    }
    else if (state.check(TokenType::DOUBLE_DOT)) {
        // Parent schema path: ..name or ..path.name
        state.advance();  // consume DOUBLE_DOT
        path.type = PathType::PARENT;

        // Must have at least one identifier after DOUBLE_DOT
        if (!state.isIdentifier()) {
            // Error: expected identifier after '..'
            return path;
        }

        // Collect path components
        while (state.isIdentifier()) {
            path.components.push_back(state.currentStringId());
            state.advance();

            if (state.check(TokenType::DOT)) {
                state.advance();
            } else {
                break;
            }
        }
    }
    else if (state.isIdentifier()) {
        // Could be:
        // - Unqualified: name
        // - Absolute: schema.name or schema.path.name

        // Get first identifier
        path.components.push_back(state.currentStringId());
        state.advance();

        // Check if there's a DOT following (making it absolute)
        if (state.check(TokenType::DOT)) {
            path.type = PathType::ABSOLUTE;

            // Collect remaining path components
            while (state.check(TokenType::DOT)) {
                state.advance();  // consume DOT

                if (!state.isIdentifier()) {
                    // Error: expected identifier after '.'
                    break;
                }

                path.components.push_back(state.currentStringId());
                state.advance();
            }
        } else {
            // Just a single identifier - unqualified
            path.type = PathType::UNQUALIFIED;
        }
    }

    // Set span from start to current position
    path.span = SourceSpan(start,
        static_cast<uint32_t>(state.currentLocation().offset - start.offset));

    return path;
}

// =============================================================================
// Table Reference Parsing
// =============================================================================

TableRef parseTableRef(ParserState& state) {
    SourceLocation start = state.currentLocation();

    // Parse the schema path
    SchemaPath path = parseSchemaPath(state);

    TableRef ref;
    ref.path = std::move(path);

    // Check for optional alias: [ AS ] identifier
    // Note: AS is a Gatekeeper keyword (KW_AS), not a contextual keyword
    if (state.match(TokenType::KW_AS)) {
        // Explicit AS keyword
        if (state.isIdentifier()) {
            ref.alias = state.currentStringId();
            ref.has_alias = true;
            state.advance();
        }
    } else if (state.isIdentifier()) {
        // Implicit alias (no AS keyword)
        // Need to be careful here - could be start of next clause
        // For now, treat any identifier as alias
        ref.alias = state.currentStringId();
        ref.has_alias = true;
        state.advance();
    }

    ref.span = SourceSpan(start,
        static_cast<uint32_t>(state.currentLocation().offset - start.offset));

    return ref;
}

// =============================================================================
// Column Reference Parsing
// =============================================================================

ColumnRef parseColumnRef(ParserState& state) {
    SourceLocation start = state.currentLocation();

    // Special case: path-qualified column (.table.column, ..table.column)
    if (state.check(TokenType::DOT) || state.check(TokenType::DOUBLE_DOT)) {
        // Parse as schema path, then extract column
        SchemaPath full_path = parseSchemaPath(state);

        if (full_path.components.empty()) {
            // Error case
            ColumnRef ref;
            ref.span = SourceSpan(start, 0);
            return ref;
        }

        // Last component is column name, rest is table path
        ColumnRef ref;
        ref.column_name = full_path.components.back();

        if (full_path.components.size() > 1) {
            ref.has_table_qualifier = true;
            ref.table_path.type = full_path.type;
            ref.table_path.components.assign(
                full_path.components.begin(),
                full_path.components.end() - 1
            );
        }

        ref.span = SourceSpan(start,
            static_cast<uint32_t>(state.currentLocation().offset - start.offset));
        return ref;
    }

    // Must start with identifier
    if (!state.isIdentifier()) {
        ColumnRef ref;
        ref.span = SourceSpan(start, 0);
        return ref;
    }

    // Collect identifiers separated by dots
    std::vector<StringPool::StringId> parts;
    parts.push_back(state.currentStringId());
    state.advance();

    while (state.check(TokenType::DOT)) {
        state.advance();  // consume DOT

        if (!state.isIdentifier()) {
            break;  // Error: expected identifier after '.'
        }

        parts.push_back(state.currentStringId());
        state.advance();
    }

    ColumnRef ref;
    ref.span = SourceSpan(start,
        static_cast<uint32_t>(state.currentLocation().offset - start.offset));

    if (parts.size() == 1) {
        // Simple column reference: column_name
        ref.column_name = parts[0];
        ref.has_table_qualifier = false;
    } else {
        // Qualified: table.column or schema.table.column
        // Last part is column, rest is table path
        ref.column_name = parts.back();
        ref.has_table_qualifier = true;

        if (parts.size() == 2) {
            // table.column - unqualified table reference
            ref.table_path.type = PathType::UNQUALIFIED;
            ref.table_path.components.push_back(parts[0]);
        } else {
            // schema.table.column or schema.path.table.column - absolute
            ref.table_path.type = PathType::ABSOLUTE;
            ref.table_path.components.assign(parts.begin(), parts.end() - 1);
        }
    }

    return ref;
}

} // namespace scratchbird::parser::v2
