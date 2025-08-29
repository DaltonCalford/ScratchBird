#include "scratchbird/engine.h"
#include "test_db_utils.h"
#include <thread>
#include <vector>
#include <cassert>

int main() {
    using namespace scratchbird;
    using namespace scratchbird::engine;
    scratchbird::tests::TestDatabaseRAII test_db("system_multi", true);

    auto worker = [&](int){
        Status st{};
        auto db = open_database(test_db.path().c_str(), st);
        auto sess = create_session(db, st);
        auto tx = begin_transaction(sess, st);
        (void)commit(tx);
        close_database(db);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back(worker, i);
    for (auto& t : threads) t.join();
    return 0;
}

