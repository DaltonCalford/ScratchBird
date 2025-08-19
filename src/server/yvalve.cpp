#include "scratchbird/yvalve.h"

namespace scratchbird
{

    bool dispatch_connect(const ConnectInfo& info, ProviderKind kind)
    {
        auto prov = get_provider(kind);
        if (!prov)
            return false;
        return prov->connect(info);
    }

} // namespace scratchbird
