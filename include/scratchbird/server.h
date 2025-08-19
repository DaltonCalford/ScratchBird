#ifndef SCRATCHBIRD_SERVER_H
#define SCRATCHBIRD_SERVER_H

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace scratchbird
{

    class Server
    {
      public:
        Server();
        ~Server();

        bool start(const std::string& bind_address, std::uint16_t port);
        void stop();

      private:
        void start_background_jobs();
        void stop_background_jobs();
        bool bg_running_{false};
        std::thread bg_thread_{};
    };

} // namespace scratchbird

#endif // SCRATCHBIRD_SERVER_H
