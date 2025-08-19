#ifndef SCRATCHBIRD_ENGINE_PARSER_H
#define SCRATCHBIRD_ENGINE_PARSER_H

#include "scratchbird/engine/ast.h"

#include <string>

namespace scratchbird
{
    namespace engine
    {

        Ast parse_sql(const std::string& sql);

    } // namespace engine
} // namespace scratchbird

#endif // SCRATCHBIRD_ENGINE_PARSER_H
