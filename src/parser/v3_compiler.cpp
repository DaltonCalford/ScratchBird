#include "scratchbird/parser/v3_compiler.h"

#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/sblr/v3_container.h"

namespace scratchbird::parser::v3 {

CompileResult Compiler::compile(std::string_view sql) {
    CompileResult result;

    parser::v3::Parser parser(sql);
    auto parse_result = parser.parseStatement();
    if (!parse_result.success()) {
        result.ok = false;
        if (!parse_result.errors().empty()) {
            result.error = parse_result.errors().front().message;
        } else {
            result.error = "parse failed";
        }
        return result;
    }

    parser::v3::V3Emitter emitter(parser.stringPool());
    scratchbird::sblr::v3::Container container;
    std::string err;
    if (!emitter.emitStatementToContainer(parse_result.statement(), container, err)) {
        result.ok = false;
        result.error = err;
        return result;
    }

    std::vector<uint8_t> encoded;
    if (!scratchbird::sblr::v3::encodeContainer(container, encoded, err)) {
        result.ok = false;
        result.error = err;
        return result;
    }

    result.ok = true;
    result.bytecode = std::move(encoded);
    return result;
}

}  // namespace scratchbird::parser::v3
