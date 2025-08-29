#include "scratchbird/engine.h"
#include "scratchbird/engine/heap_rel.h"
#include "test_db_utils.h"
#include <chrono>
#include <iostream>

int main() {
    using namespace scratchbird;
    using namespace scratchbird::engine;

    scratchbird::tests::TestDatabaseRAII test_db("perf_heap_scan", true);
    Status st{};
    auto db = open_database(test_db.path().c_str(), st);
    auto sess = create_session(db, st);
    auto tx = begin_transaction(sess, st);
    auto start = std::chrono::high_resolution_clock::now();
    // placeholder: scan once scanning API is available
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    (void)commit(tx);
    close_database(db);
    std::cout << "heap_scan_ms " << ms << "\n";
    return 0;
}

