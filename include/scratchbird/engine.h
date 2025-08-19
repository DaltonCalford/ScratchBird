#ifndef SCRATCHBIRD_ENGINE_H
#define SCRATCHBIRD_ENGINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird
{

    enum class StatusCode { Ok, NotImplemented, Error };

    struct Status {
        StatusCode code{StatusCode::Ok};
        std::string message;
    };

    struct Database;
    struct Session;
    struct Transaction;
    struct Statement;

    struct CreateDbOptions {
        std::uint32_t page_size{0};
        std::string default_charset{"UTF8"};
        std::uint32_t page_cache{0};
        std::uint32_t sweep_interval{0};
        std::uint8_t reserve_space{0};
    };

    std::shared_ptr<Database> create_database(const std::string& path, const CreateDbOptions& opts,
                                              Status& status);
    std::shared_ptr<Database> open_database(const std::string& path, Status& status);
    void close_database(std::shared_ptr<Database>& db);

    std::shared_ptr<Session> create_session(std::shared_ptr<Database> db, Status& status);

    std::shared_ptr<Transaction> begin_transaction(std::shared_ptr<Session> s, Status& status);
    Status commit(std::shared_ptr<Transaction> t);
    Status rollback(std::shared_ptr<Transaction> t);

    std::shared_ptr<Statement> prepare(std::shared_ptr<Session> s, const std::string& sql,
                                       Status& status);
    Status execute(std::shared_ptr<Statement> st, const std::vector<std::string>& params);

} // namespace scratchbird

#endif // SCRATCHBIRD_ENGINE_H
