#include "scratchbird/auth.h"

#include <cassert>
#include <memory>

using namespace scratchbird;

namespace
{
    class AllowAll : public IAuthenticator
    {
      public:
        const char* name() const override
        {
            return "allow_all";
        }
        bool authenticate(const Credentials&) override
        {
            return true;
        }
    };
} // namespace

int main()
{
    register_authenticator(std::make_shared<AllowAll>());
    Credentials c{};
    c.user = "u";
    c.password = "p";
    bool ok = authenticate(c);
    assert(ok);
    return 0;
}
