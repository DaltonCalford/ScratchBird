#ifndef SCRATCHBIRD_LISTENER_H
#define SCRATCHBIRD_LISTENER_H

#include <cstdint>
#include <string>

namespace scratchbird
{

    struct ListenerConfig {
        std::string bind_address;
        std::uint16_t port{0};
    };

    class Listener
    {
      public:
        Listener() = default;
        bool start(const ListenerConfig& cfg);
        void stop();
    };

} // namespace scratchbird

#endif // SCRATCHBIRD_LISTENER_H
