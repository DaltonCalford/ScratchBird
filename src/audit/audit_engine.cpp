#include "scratchbird/audit/audit_engine.h"

#include <algorithm>
#include <chrono>

namespace scratchbird::audit
{

    AuditEngine& AuditEngine::instance()
    {
        static AuditEngine inst;
        return inst;
    }

    void AuditEngine::set_policy(AuditPolicy policy)
    {
        std::lock_guard<std::mutex> l(mu_);
        policy_ = std::move(policy);
    }

    AuditPolicy AuditEngine::policy() const
    {
        std::lock_guard<std::mutex> l(mu_);
        return policy_;
    }

    bool AuditEngine::is_enabled(AuditEventKind kind) const
    {
        switch (kind) {
        case AuditEventKind::DDL:
            return policy_.ddl;
        case AuditEventKind::DML:
            return policy_.dml;
        case AuditEventKind::Select:
            return policy_.select;
        case AuditEventKind::Admin:
            return policy_.admin;
        }
        return true;
    }

    void AuditEngine::record(AuditEventKind kind, std::string user, std::string object,
                             std::string operation, std::string detail)
    {
        std::lock_guard<std::mutex> l(mu_);
        if (!is_enabled(kind))
            return;
        AuditEvent ev;
        ev.id = next_id_.fetch_add(1, std::memory_order_relaxed);
        ev.kind = kind;
        ev.user = std::move(user);
        ev.object = std::move(object);
        ev.operation = std::move(operation);
        ev.detail = std::move(detail);
        ev.ts_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
        buffer_.push_back(std::move(ev));
    }

    std::vector<AuditEvent> AuditEngine::recent(std::size_t limit) const
    {
        std::lock_guard<std::mutex> l(mu_);
        std::vector<AuditEvent> out;
        if (limit == 0 || buffer_.size() <= limit)
            return buffer_;
        out.insert(out.end(), buffer_.end() - limit, buffer_.end());
        return out;
    }

    void AuditEngine::clear()
    {
        std::lock_guard<std::mutex> l(mu_);
        buffer_.clear();
    }

} // namespace scratchbird::audit
