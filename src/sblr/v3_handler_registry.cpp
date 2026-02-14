#include "scratchbird/sblr/v3_handler_registry.h"

namespace scratchbird::sblr::v3 {

HandlerRegistry& HandlerRegistry::instance() {
    static HandlerRegistry registry;
    return registry;
}

void HandlerRegistry::registerStatementHandler(std::string canonical_opcode_symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    statement_handlers_.insert(std::move(canonical_opcode_symbol));
}

bool HandlerRegistry::hasStatementHandler(std::string_view canonical_opcode_symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return statement_handlers_.find(std::string(canonical_opcode_symbol)) != statement_handlers_.end();
}

std::size_t HandlerRegistry::statementHandlerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return statement_handlers_.size();
}

void HandlerRegistry::clearForTests() {
    std::lock_guard<std::mutex> lock(mutex_);
    statement_handlers_.clear();
}

}  // namespace scratchbird::sblr::v3

