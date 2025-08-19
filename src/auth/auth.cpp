#include "scratchbird/auth.h"

#include <mutex>
#include <vector>

namespace scratchbird
{

    static std::mutex g_mutex;
    static std::vector<std::shared_ptr<IAuthenticator>> g_auth;

    void register_authenticator(std::shared_ptr<IAuthenticator> a)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_auth.push_back(std::move(a));
    }

    bool authenticate(const Credentials& c)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const auto& a : g_auth) {
            if (a->authenticate(c))
                return true;
        }
        return false;
    }

} // namespace scratchbird
