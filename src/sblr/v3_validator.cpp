#include "scratchbird/sblr/v3_validator.h"

#include <utility>

#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_semantics.h"

namespace scratchbird::sblr::v3 {

namespace {

ValidationResult makeFailure(std::string code,
                             std::string message,
                             std::size_t instruction_offset = 0,
                             uint16_t opcode = 0) {
    ValidationResult result;
    result.ok = false;
    result.code = std::move(code);
    result.message = std::move(message);
    result.instruction_offset = instruction_offset;
    result.opcode = opcode;
    if (opcode != 0) {
        result.canonical_opcode_symbol = canonicalOpcodeSymbolForOpcode(opcode);
    }
    return result;
}

ValidationResult makeSuccess() {
    ValidationResult result;
    result.ok = true;
    result.code = "SBLR-E-0000";
    result.message = "ok";
    return result;
}

bool adjustStack(int& stack, int delta) {
    if (delta < 0 && stack < -delta) {
        return false;
    }
    stack += delta;
    return true;
}

size_t listSize(const Value& v) {
    if (auto list = std::get_if<Value::List>(&v.data)) return list->size();
    return 0;
}

const Value* getField(const Value& v, const std::string& name) {
    if (auto obj = std::get_if<Value::Object>(&v.data)) {
        auto it = obj->find(name);
        if (it != obj->end()) return &it->second;
    }
    return nullptr;
}

bool checkAlignment(const std::vector<SectionEntry>& sections) {
    for (const auto& s : sections) {
        if (s.offset % 8 != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

ValidationResult validateContainerDetailed(const uint8_t* data, size_t size) {
    Container container;
    std::string err;
    if (!decodeContainer(data, size, container, err)) {
        return makeFailure("SBLR-E-0001", "container decode failed: " + err);
    }

    if (!checkAlignment(container.sections)) {
        return makeFailure("SBLR-E-0002", "section table alignment violation");
    }

    if (container.bytecode_stream.empty()) {
        return makeFailure("SBLR-E-0003", "empty bytecode stream");
    }

    // Walk bytecode stream
    size_t off = 0;
    bool first = true;
    bool saw_end = false;
    int expr_stack = 0;
    while (off < container.bytecode_stream.size()) {
        const size_t instruction_offset = off;
        Instruction inst;
        DecodeError derr;
        if (!decodeInstructionWithSchema(container.bytecode_stream.data(), container.bytecode_stream.size(), off, inst, derr)) {
            return makeFailure("SBLR-E-0010",
                               "instruction decode failed: " + derr.message,
                               instruction_offset);
        }

        if (!isKnownOpcode(inst.opcode)) {
            return makeFailure("SBLR-E-0011", "unknown opcode", instruction_offset, inst.opcode);
        }

        // Semantics descriptor presence
        (void)getOpcodeSemantics(inst.opcode);

        const char* opname = opcodeName(inst.opcode);
        if (first) {
            if (!opname || std::string(opname) != "SBLR3_VERSION") {
                return makeFailure("SBLR-E-0012",
                                   "bytecode must start with SBLR3_VERSION/OP_MOD_BEGIN",
                                   instruction_offset,
                                   inst.opcode);
            }
            first = false;
        }

        if (opname && std::string(opname) == "SBLR3_VERSION") {
            if (std::holds_alternative<Value::Bytes>(inst.payload.data)) {
                const auto& b = std::get<Value::Bytes>(inst.payload.data);
                if (b.size() != 6) {
                    return makeFailure("SBLR-E-0013",
                                       "SBLR3_VERSION payload size invalid",
                                       instruction_offset,
                                       inst.opcode);
                }
            }
        }

        if (opname && std::string(opname) == "SBLR3_EXTENDED_OPCODE") {
            if (std::holds_alternative<Value::Bytes>(inst.payload.data)) {
                const auto& b = std::get<Value::Bytes>(inst.payload.data);
                if (b.size() < 2) {
                    return makeFailure("SBLR-E-0014",
                                       "SBLR3_EXTENDED_OPCODE payload too small",
                                       instruction_offset,
                                       inst.opcode);
                }
            }
        }


        // Expression stack checks (best-effort)
        const SchemaDef* schema = schemaForOpcode(inst.opcode);
        if (schema) {
            const std::string sname = schema->name;
            if (sname.rfind("SCHEMA_LITERAL_", 0) == 0) {
                if (!adjustStack(expr_stack, 1)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }
            } else if (sname == "SCHEMA_EXPR_UNARY") {
                if (!adjustStack(expr_stack, 0)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }  // pop1 push1
            } else if (sname == "SCHEMA_EXPR_BINARY") {
                if (!adjustStack(expr_stack, -1)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }  // pop2 push1
            } else if (sname == "SCHEMA_EXPR_CAST") {
                if (!adjustStack(expr_stack, 0)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }
            } else if (sname == "SCHEMA_EXPR_IN") {
                auto* rhs = getField(inst.payload, "list");
                size_t n = rhs ? listSize(*rhs) : 0;
                if (!adjustStack(expr_stack, -static_cast<int>(n))) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }
            } else if (sname == "SCHEMA_EXPR_BETWEEN") {
                if (!adjustStack(expr_stack, -2)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }  // pop3 push1
            } else if (sname == "SCHEMA_EXPR_LIKE") {
                if (!adjustStack(expr_stack, -1)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }
            } else if (sname == "SCHEMA_EXPR_EXISTS") {
                if (!adjustStack(expr_stack, 1)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }
            } else if (sname == "SCHEMA_EXPR_SUBQUERY") {
                if (!adjustStack(expr_stack, 1)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }
            } else if (sname == "SCHEMA_FUNC_CALL" || sname == "SCHEMA_AGG_CALL" || sname == "SCHEMA_WINDOW_CALL") {
                auto* args = getField(inst.payload, "args");
                size_t n = args ? listSize(*args) : 0;
                if (!adjustStack(expr_stack, -static_cast<int>(n) + 1)) {
                    return makeFailure("SBLR-E-0020", "expression stack underflow", instruction_offset, inst.opcode);
                }
            }
        }

        if (opname && std::string(opname) == "SBLR3_END") {
            saw_end = true;
            if (off != container.bytecode_stream.size()) {
                return makeFailure("SBLR-E-0015", "bytes after SBLR3_END/OP_MOD_END", instruction_offset, inst.opcode);
            }
        }
    }

    if (!saw_end) {
        return makeFailure("SBLR-E-0016", "bytecode must end with SBLR3_END/OP_MOD_END");
    }

    return makeSuccess();
}

bool validateContainer(const uint8_t* data, size_t size, std::string& err) {
    const ValidationResult result = validateContainerDetailed(data, size);
    if (result.ok) {
        return true;
    }
    err = result.code + ": " + result.message;
    if (!result.canonical_opcode_symbol.empty()) {
        err += " [";
        err += result.canonical_opcode_symbol;
        err += "]";
    }
    return false;
}

}  // namespace scratchbird::sblr::v3
