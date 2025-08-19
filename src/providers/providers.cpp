#include "scratchbird/provider.h"

#include <map>
#include <mutex>

namespace scratchbird
{

    namespace
    {
        std::mutex g_mutex;
        std::map<ProviderKind, std::shared_ptr<IProvider>> g_providers;

        class EmbeddedProvider : public IProvider
        {
          public:
            const char* name() const override
            {
                return "embedded";
            }
            bool connect(const ConnectInfo&) override
            {
                return true;
            }
        };

        class RemoteProvider : public IProvider
        {
          public:
            const char* name() const override
            {
                return "remote";
            }
            bool connect(const ConnectInfo&) override
            {
                return true;
            }
        };

        class LegacyProvider : public IProvider
        {
          public:
            const char* name() const override
            {
                return "legacy";
            }
            bool connect(const ConnectInfo&) override
            {
                return true;
            }
        };

        class OdbcProvider : public IProvider
        {
          public:
            const char* name() const override
            {
                return "odbc";
            }
            bool connect(const ConnectInfo&) override
            {
                return true;
            }
            bool can_read() const override
            {
                return true;
            }
        };

        class MssqlProvider : public IProvider
        {
          public:
            const char* name() const override
            {
                return "mssql";
            }
            bool connect(const ConnectInfo&) override
            {
                return true;
            }
            bool can_read() const override
            {
                return true;
            }
        };

        void ensure_defaults()
        {
            if (!g_providers.count(ProviderKind::Embedded)) {
                g_providers[ProviderKind::Embedded] = std::make_shared<EmbeddedProvider>();
            }
            if (!g_providers.count(ProviderKind::Remote)) {
                g_providers[ProviderKind::Remote] = std::make_shared<RemoteProvider>();
            }
            if (!g_providers.count(ProviderKind::Legacy)) {
                g_providers[ProviderKind::Legacy] = std::make_shared<LegacyProvider>();
            }
            if (!g_providers.count(ProviderKind::Odbc)) {
                g_providers[ProviderKind::Odbc] = std::make_shared<OdbcProvider>();
            }
            if (!g_providers.count(ProviderKind::Mssql)) {
                g_providers[ProviderKind::Mssql] = std::make_shared<MssqlProvider>();
            }
        }
    } // namespace

    void register_provider(ProviderKind kind, std::shared_ptr<IProvider> provider)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_providers[kind] = std::move(provider);
    }

    std::shared_ptr<IProvider> get_provider(ProviderKind kind)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        ensure_defaults();
        auto it = g_providers.find(kind);
        if (it != g_providers.end()) {
            return it->second;
        }
        return {};
    }

} // namespace scratchbird
