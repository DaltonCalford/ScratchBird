#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace scratchbird {
namespace sblr {

// ===== Value Implementation =====

int64_t Value::toInt64() const {
    switch (type) {
        case INTEGER: return std::get<int32_t>(data);
        case BIGINT: return std::get<int64_t>(data);
        case DOUBLE: return static_cast<int64_t>(std::get<double>(data));
        case BOOLEAN: return std::get<bool>(data) ? 1 : 0;
        default: return 0;
    }
}

double Value::toDouble() const {
    switch (type) {
        case INTEGER: return std::get<int32_t>(data);
        case BIGINT: return std::get<int64_t>(data);
        case DOUBLE: return std::get<double>(data);
        case BOOLEAN: return std::get<bool>(data) ? 1.0 : 0.0;
        default: return 0.0;
    }
}

std::string Value::toString() const {
    switch (type) {
        case NULL_VALUE: return "NULL";
        case INTEGER: return std::to_string(std::get<int32_t>(data));
        case BIGINT: return std::to_string(std::get<int64_t>(data));
        case DOUBLE: {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6) << std::get<double>(data);
            return ss.str();
        }
        case STRING: return std::get<std::string>(data);
        case BOOLEAN: return std::get<bool>(data) ? "true" : "false";
        default: return "";
    }
}

bool Value::toBoolean() const {
    switch (type) {
        case NULL_VALUE: return false;
        case INTEGER: return std::get<int32_t>(data) != 0;
        case BIGINT: return std::get<int64_t>(data) != 0;
        case DOUBLE: return std::get<double>(data) != 0.0;
        case STRING: return !std::get<std::string>(data).empty();
        case BOOLEAN: return std::get<bool>(data);
        default: return false;
    }
}

// ===== ResultSet Implementation =====

void ResultSet::addColumn(const std::string& name, core::DataType type) {
    column_names_.push_back(name);
    column_types_.push_back(type);
}

void ResultSet::addRow(std::vector<Value> row) {
    if (row.size() != column_names_.size()) {
        throw std::runtime_error("Row column count mismatch");
    }
    rows_.push_back(std::move(row));
}

void ResultSet::print(std::ostream& out) const {
    // Print column headers
    for (size_t i = 0; i < column_names_.size(); i++) {
        if (i > 0) out << " | ";
        out << std::setw(15) << column_names_[i];
    }
    out << "\n";
    
    // Print separator
    for (size_t i = 0; i < column_names_.size(); i++) {
        if (i > 0) out << "-+-";
        out << std::string(15, '-');
    }
    out << "\n";
    
    // Print rows
    for (const auto& row : rows_) {
        for (size_t i = 0; i < row.size(); i++) {
            if (i > 0) out << " | ";
            out << std::setw(15) << row[i].toString();
        }
        out << "\n";
    }
    
    out << "(" << rows_.size() << " rows)\n";
}

// ===== Executor Implementation =====

Executor::Executor(core::Database* db) : db_(db), pc_(0) {}

Executor::~Executor() = default;

ExecutionResult Executor::execute(const std::vector<uint8_t>& bytecode) {
    // Reset execution state
    bytecode_ = bytecode.data();
    bytecode_size_ = bytecode.size();
    pc_ = 0;
    while (!stack_.empty()) stack_.pop();
    strings_.clear();
    current_table_.clear();
    current_columns_.clear();
    current_result_set_.reset();
    
    try {
        // Check version
        if (readByte() != static_cast<uint8_t>(Opcode::VERSION)) {
            return ExecutionResult("Invalid bytecode: missing version");
        }
        
        uint8_t version = readByte();
        if (version != SBLR_VERSION) {
            return ExecutionResult("Unsupported bytecode version: " + 
                                 std::to_string(version));
        }
        
        // Execute main statement
        Opcode op = static_cast<Opcode>(readByte());
        switch (op) {
            case Opcode::CREATE_TABLE:
                executeCreateTable();
                return ExecutionResult();
                
            case Opcode::INSERT:
                executeInsert();
                return ExecutionResult();
                
            case Opcode::SELECT:
                executeSelect();
                return ExecutionResult(std::move(current_result_set_));
                
            default:
                return ExecutionResult("Unknown statement opcode: " + 
                                     std::to_string(static_cast<int>(op)));
        }
        
    } catch (const std::exception& e) {
        return ExecutionResult(std::string("Execution error: ") + e.what());
    }
}

uint8_t Executor::readByte() {
    if (pc_ >= bytecode_size_) {
        throw std::runtime_error("Bytecode underflow");
    }
    return bytecode_[pc_++];
}

uint32_t Executor::readInt32() {
    if (pc_ + 4 > bytecode_size_) {
        throw std::runtime_error("Bytecode underflow");
    }
    uint32_t value = sblr::readInt32(&bytecode_[pc_]);
    pc_ += 4;
    return value;
}

uint64_t Executor::readInt64() {
    if (pc_ + 8 > bytecode_size_) {
        throw std::runtime_error("Bytecode underflow");
    }
    uint64_t value = sblr::readInt64(&bytecode_[pc_]);
    pc_ += 8;
    return value;
}

double Executor::readDouble() {
    if (pc_ + 8 > bytecode_size_) {
        throw std::runtime_error("Bytecode underflow");
    }
    double value;
    std::memcpy(&value, &bytecode_[pc_], 8);
    pc_ += 8;
    return value;
}

std::string Executor::readString() {
    uint32_t length = readInt32();
    if (pc_ + length > bytecode_size_) {
        throw std::runtime_error("Bytecode underflow");
    }
    std::string str(reinterpret_cast<const char*>(&bytecode_[pc_]), length);
    pc_ += length;
    return str;
}

Value Executor::pop() {
    if (stack_.empty()) {
        throw std::runtime_error("Stack underflow");
    }
    Value v = stack_.top();
    stack_.pop();
    return v;
}

// Minimal implementations for now
void Executor::executeCreateTable() {
    throw std::runtime_error("CREATE TABLE not yet implemented");
}

void Executor::executeInsert() {
    throw std::runtime_error("INSERT not yet implemented");
}

void Executor::executeSelect() {
    throw std::runtime_error("SELECT not yet implemented");
}

void Executor::evaluateExpression() {
    throw std::runtime_error("Expression evaluation not yet implemented");
}

void Executor::executeBinaryOp(Opcode op) {
    throw std::runtime_error("Binary operations not yet implemented");
}

} // namespace sblr
} // namespace scratchbird