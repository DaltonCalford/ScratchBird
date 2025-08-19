// SPDX-License-Identifier: IDPL
#ifndef SCRATCHBIRD_ENGINE_PARSER_PSQL_H
#define SCRATCHBIRD_ENGINE_PARSER_PSQL_H

#include "scratchbird/engine/ast.h"

#include <string>

namespace scratchbird::engine
{
    Ast parse_psql_block(const std::string& sql);
    Ast parse_psql_execstmt(const std::string& sql);
    Ast parse_psql_routine(const std::string& sql);
    Ast parse_psql_trigger(const std::string& sql);
    Ast parse_psql_package(const std::string& sql);
} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_PARSER_PSQL_H
