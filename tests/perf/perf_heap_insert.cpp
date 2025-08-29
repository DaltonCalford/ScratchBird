#include "scratchbird/engine.h"
#include "scratchbird/engine/heap_rel.h"
#include "../test_db_utils.h"
#include <chrono>
#include <iostream>

int main() {
    using namespace scratchbird;
    using namespace scratchbird::engine;

    scratchbird::tests::TestDatabaseRAII test_db("perf_heap_insert", true);
    Status st{};
    auto db = open_database(test_db.path().c_str(), st);
    auto sess = create_session(db, st);
    auto tx = begin_transaction(sess, st);

    const int num_rows = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_rows; ++i) {
        (void)i; // placeholder for insert once engine has SQL insert API
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    (void)commit(tx);
    close_database(db);
    std::cout << "heap_insert_ms " << ms << "\n";
    return 0;
}

