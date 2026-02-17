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

bool isVNextOpcodeRange(uint16_t opcode) {
    return opcode >= 0x6000 && opcode <= 0x60FF;
}

const Value* findField(const Value::Object& payload, const char* field_name) {
    auto it = payload.find(field_name);
    return it == payload.end() ? nullptr : &it->second;
}

bool readU64Field(const Value::Object& payload,
                  const char* field_name,
                  uint64_t& out,
                  std::string& err) {
    const Value* value = findField(payload, field_name);
    if (!value) {
        err = std::string("missing required field: ") + field_name;
        return false;
    }
    const auto* typed = std::get_if<uint64_t>(&value->data);
    if (!typed) {
        err = std::string("invalid field type for: ") + field_name;
        return false;
    }
    out = *typed;
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

ValidationResult validateVNextOpcodeContract(const Instruction& inst) {
    if (!isVNextOpcodeRange(inst.opcode)) {
        return makeSuccess();
    }
    if (!isKnownOpcode(inst.opcode)) {
        return makeFailure("IRX_0403", "unknown SBLR vNext opcode", 0, inst.opcode);
    }

    const SchemaDef* schema = schemaForOpcode(inst.opcode);
    if (!schema) {
        return makeFailure("IRX_0404",
                           "vNext opcode missing payload schema",
                           0,
                           inst.opcode);
    }

    const auto* payload = std::get_if<Value::Object>(&inst.payload.data);
    if (!payload) {
        return makeFailure("IRX_0404", "vNext payload must be an object", 0, inst.opcode);
    }

    uint64_t enum_value = 0;
    std::string field_err;
    switch (inst.opcode) {
        case static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER):
            if (!readU64Field(*payload, "cmp", enum_value, field_err)) {
                return makeFailure("IRX_0404", field_err, 0, inst.opcode);
            }
            if (enum_value > 7) {
                return makeFailure("IRX_0407", "cmp enum out of range", 0, inst.opcode);
            }
            break;
        case static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL):
            if (!readU64Field(*payload, "scorer_id", enum_value, field_err)) {
                return makeFailure("IRX_0404", field_err, 0, inst.opcode);
            }
            if (enum_value < 1 || enum_value > 3) {
                return makeFailure("IRX_0407", "scorer_id enum out of range", 0, inst.opcode);
            }
            break;
        case static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN):
            if (!readU64Field(*payload, "metric", enum_value, field_err)) {
                return makeFailure("IRX_0404", field_err, 0, inst.opcode);
            }
            if (enum_value < 1 || enum_value > 3) {
                return makeFailure("IRX_0407", "metric enum out of range", 0, inst.opcode);
            }
            break;
        case static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE):
            if (!readU64Field(*payload, "mode", enum_value, field_err)) {
                return makeFailure("IRX_0404", field_err, 0, inst.opcode);
            }
            if (enum_value < 1 || enum_value > 3) {
                return makeFailure("IRX_0407", "mode enum out of range", 0, inst.opcode);
            }
            break;
        case static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE):
            if (!readU64Field(*payload, "buffer_class", enum_value, field_err)) {
                return makeFailure("IRX_0404", field_err, 0, inst.opcode);
            }
            if (enum_value < 1 || enum_value > 4) {
                return makeFailure("IRX_0407", "buffer_class enum out of range", 0, inst.opcode);
            }
            break;
        default:
            break;
    }

    return makeSuccess();
}

ValidationResult validateVNextEncodedInstructionContract(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 8) {
        return makeFailure("IRX_0404", "instruction header too short");
    }

    const uint16_t opcode = static_cast<uint16_t>(data[0]) |
                            (static_cast<uint16_t>(data[1]) << 8);
    if (!isVNextOpcodeRange(opcode)) {
        return makeSuccess();
    }
    if (!isKnownOpcode(opcode)) {
        return makeFailure("IRX_0403", "unknown SBLR vNext opcode", 0, opcode);
    }

    Instruction decoded;
    DecodeError err;
    size_t offset = 0;
    if (!decodeInstructionWithSchema(data, size, offset, decoded, err)) {
        return makeFailure("IRX_0404",
                           std::string("invalid opcode payload length: ") + err.message,
                           0,
                           opcode);
    }
    if (offset != size) {
        return makeFailure("IRX_0404",
                           "instruction payload has trailing bytes",
                           0,
                           opcode);
    }

    return validateVNextOpcodeContract(decoded);
}

ValidationResult validateVNextRewriteEvidenceContract(const Value::Object& evidence) {
    auto hasString = [&](const char* field) -> bool {
        auto it = evidence.find(field);
        return it != evidence.end() && std::holds_alternative<std::string>(it->second.data);
    };
    auto hasU64 = [&](const char* field) -> bool {
        auto it = evidence.find(field);
        return it != evidence.end() && std::holds_alternative<uint64_t>(it->second.data);
    };

    if (!hasString("rewrite_rule_id") ||
        !hasString("source_token_span") ||
        !hasString("target_node_symbol") ||
        !hasU64("deterministic_hash64")) {
        return makeFailure("IRX_0405",
                           "rewrite evidence missing required metadata fields");
    }
    return makeSuccess();
}

}  // namespace scratchbird::sblr::v3
