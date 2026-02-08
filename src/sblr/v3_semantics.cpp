#include "scratchbird/sblr/v3_semantics.h"

#include <string>

#include "scratchbird/sblr/v3_opcode_registry.h"
#include "scratchbird/sblr/v3_payloads.h"

namespace scratchbird::sblr::v3 {

static bool nameStartsWith(const std::string& s, const std::string& pfx) {
    return s.rfind(pfx, 0) == 0;
}

OpcodeSemantics getOpcodeSemantics(uint16_t opcode) {
    OpcodeSemantics sem;
    const char* name_c = opcodeName(opcode);
    if (!name_c) return sem;
    std::string name(name_c);

    const SchemaDef* schema = schemaForOpcode(opcode);
    if (schema) {
        if (schema->name.rfind("SCHEMA_LITERAL_", 0) == 0) {
            sem.is_expression = true;
            sem.stack_in = 0;
            sem.stack_out = 1;
            return sem;
        }
        if (schema->name.rfind("SCHEMA_EXPR_", 0) == 0 ||
            schema->name == "SCHEMA_FUNC_CALL" ||
            schema->name == "SCHEMA_AGG_CALL" ||
            schema->name == "SCHEMA_WINDOW_CALL") {
            sem.is_expression = true;
        }
        if (schema->name.rfind("SCHEMA_DDL_", 0) == 0 ||
            schema->name.rfind("SCHEMA_DML_", 0) == 0 ||
            schema->name.rfind("SCHEMA_PSQL_", 0) == 0 ||
            schema->name == "SCHEMA_SELECT" ||
            schema->name == "SCHEMA_INSERT" ||
            schema->name == "SCHEMA_UPDATE" ||
            schema->name == "SCHEMA_DELETE" ||
            schema->name == "SCHEMA_MERGE") {
            sem.is_statement = true;
        }
    }

    if (nameStartsWith(name, "SBLR3_CREATE") || nameStartsWith(name, "SBLR3_ALTER") ||
        nameStartsWith(name, "SBLR3_DROP") || nameStartsWith(name, "SBLR3_TRUNCATE") ||
        nameStartsWith(name, "SBLR3_INSERT") || nameStartsWith(name, "SBLR3_UPDATE") ||
        nameStartsWith(name, "SBLR3_DELETE") || nameStartsWith(name, "SBLR3_MERGE") ||
        name == "SBLR3_SELECT") {
        sem.requires_lock_order = true;
    }

    return sem;
}

}  // namespace scratchbird::sblr::v3
