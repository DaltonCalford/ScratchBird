#include "scratchbird/sblr/v3_validator.h"

#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_semantics.h"

namespace scratchbird::sblr::v3 {


static bool adjustStack(int& stack, int delta, std::string& err) {
    if (delta < 0 && stack < -delta) {
        err = "stack underflow";
        return false;
    }
    stack += delta;
    return true;
}

static size_t listSize(const Value& v) {
    if (auto list = std::get_if<Value::List>(&v.data)) return list->size();
    return 0;
}

static const Value* getField(const Value& v, const std::string& name) {
    if (auto obj = std::get_if<Value::Object>(&v.data)) {
        auto it = obj->find(name);
        if (it != obj->end()) return &it->second;
    }
    return nullptr;
}

static bool checkAlignment(const std::vector<SectionEntry>& sections, std::string& err) {
    for (const auto& s : sections) {
        if (s.offset % 8 != 0) {
            err = "section not 8-byte aligned";
            return false;
        }
    }
    return true;
}

bool validateContainer(const uint8_t* data, size_t size, std::string& err) {
    Container container;
    if (!decodeContainer(data, size, container, err)) {
        return false;
    }

    if (!checkAlignment(container.sections, err)) {
        return false;
    }

    if (container.bytecode_stream.empty()) {
        err = "empty bytecode stream";
        return false;
    }

    // Walk bytecode stream
    size_t off = 0;
    bool first = true;
    bool saw_end = false;
    int expr_stack = 0;
    while (off < container.bytecode_stream.size()) {
        Instruction inst;
        DecodeError derr;
        if (!decodeInstructionWithSchema(container.bytecode_stream.data(), container.bytecode_stream.size(), off, inst, derr)) {
            err = derr.message;
            return false;
        }

        if (!isKnownOpcode(inst.opcode)) {
            err = "unknown opcode";
            return false;
        }

        // Semantics descriptor presence
        (void)getOpcodeSemantics(inst.opcode);

        const char* opname = opcodeName(inst.opcode);
        if (first) {
            if (!opname || std::string(opname) != "SBLR3_VERSION") {
                err = "bytecode must start with SBLR3_VERSION";
                return false;
            }
            first = false;
        }

        if (opname && std::string(opname) == "SBLR3_VERSION") {
            if (std::holds_alternative<Value::Bytes>(inst.payload.data)) {
                const auto& b = std::get<Value::Bytes>(inst.payload.data);
                if (b.size() != 6) { err = "SBLR3_VERSION payload size"; return false; }
            }
        }

        if (opname && std::string(opname) == "SBLR3_EXTENDED_OPCODE") {
            if (std::holds_alternative<Value::Bytes>(inst.payload.data)) {
                const auto& b = std::get<Value::Bytes>(inst.payload.data);
                if (b.size() < 2) { err = "SBLR3_EXTENDED_OPCODE payload too small"; return false; }
            }
        }


        // Expression stack checks (best-effort)
        const SchemaDef* schema = schemaForOpcode(inst.opcode);
        if (schema) {
            const std::string sname = schema->name;
            if (sname.rfind("SCHEMA_LITERAL_", 0) == 0) {
                if (!adjustStack(expr_stack, 1, err)) return false;
            } else if (sname == "SCHEMA_EXPR_UNARY") {
                if (!adjustStack(expr_stack, 0, err)) return false; // pop1 push1
            } else if (sname == "SCHEMA_EXPR_BINARY") {
                if (!adjustStack(expr_stack, -1, err)) return false; // pop2 push1
            } else if (sname == "SCHEMA_EXPR_CAST") {
                if (!adjustStack(expr_stack, 0, err)) return false;
            } else if (sname == "SCHEMA_EXPR_IN") {
                auto* rhs = getField(inst.payload, "list");
                size_t n = rhs ? listSize(*rhs) : 0;
                if (!adjustStack(expr_stack, -static_cast<int>(n), err)) return false;
            } else if (sname == "SCHEMA_EXPR_BETWEEN") {
                if (!adjustStack(expr_stack, -2, err)) return false; // pop3 push1
            } else if (sname == "SCHEMA_EXPR_LIKE") {
                if (!adjustStack(expr_stack, -1, err)) return false;
            } else if (sname == "SCHEMA_EXPR_EXISTS") {
                if (!adjustStack(expr_stack, 1, err)) return false;
            } else if (sname == "SCHEMA_EXPR_SUBQUERY") {
                if (!adjustStack(expr_stack, 1, err)) return false;
            } else if (sname == "SCHEMA_FUNC_CALL" || sname == "SCHEMA_AGG_CALL" || sname == "SCHEMA_WINDOW_CALL") {
                auto* args = getField(inst.payload, "args");
                size_t n = args ? listSize(*args) : 0;
                if (!adjustStack(expr_stack, -static_cast<int>(n) + 1, err)) return false;
            }
        }

        if (opname && std::string(opname) == "SBLR3_END") {
            saw_end = true;
            if (off != container.bytecode_stream.size()) {
                err = "bytes after SBLR3_END";
                return false;
            }
        }
    }

    if (!saw_end) {
        err = "bytecode must end with SBLR3_END";
        return false;
    }

    return true;
}

}  // namespace scratchbird::sblr::v3
