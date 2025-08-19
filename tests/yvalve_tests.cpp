#include "scratchbird/yvalve.h"

#include <cassert>

using namespace scratchbird;

int main()
{
    ConnectInfo local{"/tmp/db.sbk"};
    assert(dispatch_connect(local, ProviderKind::Embedded));

    ConnectInfo remote{"host:/tmp/db.sbk"};
    assert(dispatch_connect(remote, ProviderKind::Remote));
    return 0;
}
