#include "scratchbird.h"

#include <cassert>

int main()
{
    const auto v = scratchbird::version();
    assert(!v.empty());
    return 0;
}
