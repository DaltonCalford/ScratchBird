#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

namespace scratchbird::sblr::v3 {

class HandlerRegistry {
public:
    static HandlerRegistry& instance();

    void registerStatementHandler(std::string canonical_opcode_symbol);
    bool hasStatementHandler(std::string_view canonical_opcode_symbol) const;
    std::size_t statementHandlerCount() const;

    // Test helper: clears registered handlers.
    void clearForTests();

private:
    HandlerRegistry() = default;
    HandlerRegistry(const HandlerRegistry&) = delete;
    HandlerRegistry& operator=(const HandlerRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_set<std::string> statement_handlers_;
};

}  // namespace scratchbird::sblr::v3
