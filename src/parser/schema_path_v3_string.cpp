/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/parser/schema_path_v3.h"

#include <sstream>

namespace scratchbird::parser::v3 {

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

    switch (path.type) {
        case PathType::CURRENT:
            ss << ".";
            break;
        case PathType::PARENT:
            ss << "..";
            break;
        case PathType::UNQUALIFIED:
        case PathType::ABSOLUTE:
            break;
    }

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

} // namespace scratchbird::parser::v3
