#pragma once

#include <cstdint>
#include <string>

#include "scratchbird/sblr/native_sql_render_contract.h"
#include "scratchbird/sblr/v3_types.h"

namespace scratchbird::sblr {

enum class NativeSqlObjectTypeHint : uint8_t {
    UNKNOWN = 0,
    SCHEMA = 1,
    TABLE = 2,
    INDEX = 3,
    VIEW = 4,
    POLICY = 5,
    USER = 6,
    ROLE = 7,
    GROUP = 8,
    JOB = 9,
    DATABASE = 10,
};

class NativeSqlNameResolver {
public:
    virtual ~NativeSqlNameResolver() = default;
    virtual bool resolveNameByUuid(const std::string& uuid_text,
                                   NativeSqlObjectTypeHint hint,
                                   std::string& resolved_name) = 0;
};

struct NativeSqlRenderResult {
    std::string sql;
    std::string contract_id;
    std::string canonical_opcode_symbol;
    NativeSqlResultShape result_shape = NativeSqlResultShape::COMMAND_STATUS;
};

bool renderNativeSqlInstruction(const v3::Instruction& instruction,
                                NativeSqlNameResolver* resolver,
                                NativeSqlRenderResult& out,
                                std::string& error);

bool renderNativeSqlInstruction(const v3::Instruction& instruction,
                                NativeSqlRenderResult& out,
                                std::string& error);

}  // namespace scratchbird::sblr
