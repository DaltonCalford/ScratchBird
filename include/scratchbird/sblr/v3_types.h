#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace scratchbird::sblr::v3 {

struct Instruction;

struct TypeSpec {
    uint16_t type_opcode = 0;
    std::vector<uint8_t> type_payload;  // already encoded per spec
};

struct Value {
    using Bytes = std::vector<uint8_t>;
    using List = std::vector<Value>;
    using Object = std::map<std::string, Value>;
    using InstrPtr = std::shared_ptr<Instruction>;

    using Variant = std::variant<std::monostate, bool, int64_t, uint64_t, double,
                                 std::string, Bytes, List, Object, InstrPtr, TypeSpec>;

    Variant data;

    Value() = default;
    explicit Value(bool v) : data(v) {}
    explicit Value(int64_t v) : data(v) {}
    explicit Value(uint64_t v) : data(v) {}
    explicit Value(double v) : data(v) {}
    explicit Value(std::string v) : data(std::move(v)) {}
    explicit Value(const char* v) : data(std::string(v)) {}
    explicit Value(Bytes v) : data(std::move(v)) {}
    explicit Value(List v) : data(std::move(v)) {}
    explicit Value(Object v) : data(std::move(v)) {}
    explicit Value(InstrPtr v) : data(std::move(v)) {}
    explicit Value(TypeSpec v) : data(std::move(v)) {}

    bool isNull() const { return std::holds_alternative<std::monostate>(data); }
};

struct Instruction {
    uint16_t opcode = 0;
    uint16_t flags = 0;
    Value payload;  // typically an Object keyed by field name
};

}  // namespace scratchbird::sblr::v3
