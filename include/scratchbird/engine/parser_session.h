// SPDX-License-Identifier: IDPL
#ifndef SCRATCHBIRD_ENGINE_PARSER_SESSION_H
#define SCRATCHBIRD_ENGINE_PARSER_SESSION_H

#include "scratchbird/engine/ast.h"

#include <string>

namespace scratchbird::engine
{

    // Parse a session/transaction control statement; returns Ast with kind SessionStmt
    Ast parse_session_stmt(const std::string& sql);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_PARSER_SESSION_H
