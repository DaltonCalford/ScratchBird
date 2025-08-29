#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scratchbird::audit
{

    enum class AuditEventKind : uint8_t {
        DDL,
        DML,
        Select,
        Admin,
    };

    struct AuditEvent {
        uint64_t id;
        AuditEventKind kind;
        std::string user;
        std::string object;
        std::string operation;
        std::string detail;
        uint64_t ts_epoch_ms;
    };

    struct AuditPolicy {
        std::string name;
        bool ddl = true;
        bool dml = true;
        bool select = false;
        bool admin = true;
    };

    class AuditEngine
    {
      public:
        static AuditEngine& instance();

        void set_policy(AuditPolicy policy);
        AuditPolicy policy() const;

        void record(AuditEventKind kind, std::string user, std::string object,
                    std::string operation, std::string detail);

        std::vector<AuditEvent> recent(std::size_t limit) const;
        void clear();

      private:
        AuditEngine() = default;
        bool is_enabled(AuditEventKind kind) const;

        mutable std::mutex mu_;
        AuditPolicy policy_{};
        std::vector<AuditEvent> buffer_;
        std::atomic<uint64_t> next_id_{1};
    };

} // namespace scratchbird::audit
