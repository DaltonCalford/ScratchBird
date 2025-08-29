#include "scratchbird/engine.h"
#include "../test_db_utils.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace scratchbird;
    using namespace scratchbird::engine;
    scratchbird::tests::TestDatabaseRAII test_db("db_lifecycle", true);
    Status st{};
    auto db = open_database(test_db.path().c_str(), st);
    assert(db != nullptr);
    auto sess = create_session(db, st);
    assert(sess != nullptr);
    close_database(db);
    std::cout << "db lifecycle ok\n";
    return 0;
}

