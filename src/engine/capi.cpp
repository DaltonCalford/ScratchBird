#include "scratchbird/capi.h"

#include "scratchbird/engine.h"

#include <memory>
#include <string>
#include <vector>

using namespace scratchbird;

namespace
{
    static const char* to_c_message(const Status& st)
    {
        static thread_local std::string msg_copy;
        if (st.message.empty())
            return nullptr;
        msg_copy = st.message;
        return msg_copy.c_str();
    }

    static SB_Status to_sb_status(const Status& st)
    {
        SB_StatusCode code = SB_STATUS_OK;
        switch (st.code) {
        case StatusCode::Ok:
            code = SB_STATUS_OK;
            break;
        case StatusCode::NotImplemented:
            code = SB_STATUS_NOT_IMPLEMENTED;
            break;
        default:
            code = SB_STATUS_ERROR;
            break;
        }
        return SB_Status{code, to_c_message(st)};
    }

} // namespace

// Define opaque handle structs declared in C header at global scope
struct SB_Database {
    std::shared_ptr<Database> db;
};
struct SB_Session {
    std::shared_ptr<Session> s;
};
struct SB_Transaction {
    std::shared_ptr<Transaction> t;
};
struct SB_Statement {
    std::shared_ptr<Statement> st;
};

extern "C" SB_Status sb_create_database(const char* path, const SB_CreateDbOptions* opts,
                                        SB_Database** out_db)
{
    if (!path || !out_db)
        return SB_Status{SB_STATUS_ERROR, "invalid args"};
    Status st{};
    CreateDbOptions o{};
    if (opts) {
        o.page_size = opts->page_size;
        o.default_charset = opts->default_charset ? opts->default_charset : "UTF8";
        o.page_cache = opts->page_cache;
        o.sweep_interval = opts->sweep_interval;
        o.reserve_space = opts->reserve_space;
    }
    auto db = create_database(path, o, st);
    auto* h = new SB_Database{db};
    *out_db = h;
    return to_sb_status(st);
}

extern "C" SB_Status sb_open_database(const char* path, SB_Database** out_db)
{
    if (!out_db) {
        return SB_Status{SB_STATUS_ERROR, "out_db is null"};
    }
    Status st{};
    auto db = open_database(path ? std::string(path) : std::string(), st);
    auto* h = new SB_Database{db};
    *out_db = h;
    return to_sb_status(st);
}

extern "C" void sb_close_database(SB_Database* db)
{
    if (!db)
        return;
    db->db.reset();
    delete db;
}

extern "C" SB_Status sb_create_session(SB_Database* db, SB_Session** out_session)
{
    if (!db || !out_session) {
        return SB_Status{SB_STATUS_ERROR, "invalid args"};
    }
    Status st{};
    auto s = create_session(db->db, st);
    auto* h = new SB_Session{s};
    *out_session = h;
    return to_sb_status(st);
}

extern "C" SB_Status sb_begin_transaction(SB_Session* s, SB_Transaction** out_tx)
{
    if (!s || !out_tx) {
        return SB_Status{SB_STATUS_ERROR, "invalid args"};
    }
    Status st{};
    auto t = begin_transaction(s->s, st);
    auto* h = new SB_Transaction{t};
    *out_tx = h;
    return to_sb_status(st);
}

extern "C" SB_Status sb_commit(SB_Transaction* tx)
{
    if (!tx) {
        return SB_Status{SB_STATUS_ERROR, "invalid args"};
    }
    return to_sb_status(commit(tx->t));
}

extern "C" SB_Status sb_rollback(SB_Transaction* tx)
{
    if (!tx) {
        return SB_Status{SB_STATUS_ERROR, "invalid args"};
    }
    return to_sb_status(rollback(tx->t));
}

extern "C" SB_Status sb_prepare(SB_Session* s, const char* sql, SB_Statement** out_stmt)
{
    if (!s || !out_stmt) {
        return SB_Status{SB_STATUS_ERROR, "invalid args"};
    }
    Status st{};
    auto stptr = prepare(s->s, sql ? std::string(sql) : std::string(), st);
    auto* h = new SB_Statement{stptr};
    *out_stmt = h;
    return to_sb_status(st);
}

extern "C" SB_Status sb_execute(SB_Statement* st_h, const char* const* params, int32_t num_params)
{
    if (!st_h) {
        return SB_Status{SB_STATUS_ERROR, "invalid args"};
    }
    std::vector<std::string> vec;
    if (params && num_params > 0) {
        vec.reserve(static_cast<size_t>(num_params));
        for (int32_t i = 0; i < num_params; ++i) {
            vec.emplace_back(params[i] ? params[i] : "");
        }
    }
    return to_sb_status(execute(st_h->st, vec));
}
