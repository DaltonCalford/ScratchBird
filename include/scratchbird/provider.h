#ifndef SCRATCHBIRD_PROVIDER_H
#define SCRATCHBIRD_PROVIDER_H

#include <memory>
#include <string>

namespace scratchbird
{

    enum class ProviderKind { Embedded, Remote, Legacy, Odbc, Mssql };

    struct ConnectInfo {
        std::string path_or_dsn; // local path or remote DSN
    };

    class IProvider
    {
      public:
        virtual ~IProvider() = default;
        virtual const char* name() const = 0;
        virtual bool connect(const ConnectInfo& info) = 0; // stub
        // Optional capability advertisement for pushdown
        virtual bool can_read() const
        {
            return false;
        }
        virtual bool can_write() const
        {
            return false;
        }
    };

    void register_provider(ProviderKind kind, std::shared_ptr<IProvider> provider);
    std::shared_ptr<IProvider> get_provider(ProviderKind kind);

} // namespace scratchbird

#endif // SCRATCHBIRD_PROVIDER_H
