#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "scratchbird/parser/parser_v2.h"

namespace scratchbird::parser::v3 {

struct CompileResult {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> bytecode;
};

class Compiler {
public:
    Compiler() = default;
    CompileResult compile(std::string_view sql);
};

}  // namespace scratchbird::parser::v3
