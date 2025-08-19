#ifndef SCRATCHBIRD_YVALVE_H
#define SCRATCHBIRD_YVALVE_H

#include "scratchbird/provider.h"

namespace scratchbird
{

    // Minimal dispatcher: choose provider and connect.
    bool dispatch_connect(const ConnectInfo& info, ProviderKind kind);

} // namespace scratchbird

#endif // SCRATCHBIRD_YVALVE_H
