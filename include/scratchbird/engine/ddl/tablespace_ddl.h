#ifndef SCRATCHBIRD_ENGINE_DDL_TABLESPACE_DDL_H
#define SCRATCHBIRD_ENGINE_DDL_TABLESPACE_DDL_H

#include "scratchbird/engine/ast.h"

#include <string>

namespace scratchbird::engine
{

    // Execute basic TABLESPACE DDL with validations (Milestone B minimal):
    // - CREATE TABLESPACE name LOCATION 'path' WITH (...)
    // - ALTER TABLESPACE name SET (...)
    // - DROP TABLESPACE name [IF EMPTY] (no-op for default)
    // Validations are best-effort and confined to embedded engine context.
    bool execute_tablespace_ddl(const Ast& ast, const std::string& db_path, std::string& error);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_DDL_TABLESPACE_DDL_H
