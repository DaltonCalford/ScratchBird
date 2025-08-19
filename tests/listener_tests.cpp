#include "scratchbird/listener.h"

#include <cassert>

using namespace scratchbird;

int main()
{
    Listener l;
    ListenerConfig cfg{"0.0.0.0", 0};
    assert(l.start(cfg));
    l.stop();
    return 0;
}
