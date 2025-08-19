#ifndef SCRATCHBIRD_AUTH_H
#define SCRATCHBIRD_AUTH_H

#include <memory>
#include <string>
#include <vector>

namespace scratchbird
{

    enum class AuthMethod { Password, Trusted, TwoFactor };

    struct Credentials {
        AuthMethod method{AuthMethod::Password};
        std::string user;
        std::string password;
        std::string token;
    };

    class IAuthenticator
    {
      public:
        virtual ~IAuthenticator() = default;
        virtual const char* name() const = 0;
        virtual bool authenticate(const Credentials& c) = 0;
    };

    void register_authenticator(std::shared_ptr<IAuthenticator> a);
    bool authenticate(const Credentials& c);

} // namespace scratchbird

#endif // SCRATCHBIRD_AUTH_H
