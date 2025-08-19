// SPDX-License-Identifier: IDPL
#include "scratchbird/engine/parser.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace scratchbird::engine;

static const char* ddl_seeds[] = {
    "CREATE TABLE t (id INT, a VARCHAR(10))",
    "ALTER TABLE t ALTER COLUMN a TYPE VARCHAR(20)",
    "CREATE VIEW v AS SELECT 1",
    "GRANT SELECT ON TABLE t TO user1",
    "CREATE SEQUENCE s",
    "CREATE DOMAIN d AS INT CHECK (VALUE > 0)",
};

static const char* psql_seeds[] = {
    "EXECUTE BLOCK AS BEGIN x = 1; END",
    "CREATE PROCEDURE p (a INT) RETURNS (b INT) AS BEGIN b = a; END",
    "CREATE TRIGGER tr BEFORE INSERT ON t AS BEGIN SUSPEND; END",
};

static std::string mutate(const std::string& in, std::mt19937& rng)
{
    std::uniform_int_distribution<int> dice(0, 6);
    std::string s = in;
    int ops = 10;
    for (int i = 0; i < ops; ++i) {
        int kind = dice(rng);
        if (s.empty())
            s = in;
        size_t pos = (s.size() ? (rng() % s.size()) : 0);
        switch (kind) {
        case 0:
            s.insert(pos, 1, '(');
            break;
        case 1:
            s.insert(pos, 1, ')');
            break;
        case 2:
            s.insert(pos, 1, ',');
            break;
        case 3:
            s.insert(pos, 1, '\'');
            break;
        case 4:
            if (!s.empty())
                s.erase(pos, 1);
            break;
        case 5:
            s.insert(pos, " /*x*/ ");
            break;
        case 6:
            s.insert(pos, " INTO x");
            break;
        }
    }
    return s;
}

int main()
{
    std::mt19937 rng(1234567);
    std::vector<std::string> seeds;
    for (auto* z : ddl_seeds)
        seeds.emplace_back(z);
    for (auto* z : psql_seeds)
        seeds.emplace_back(z);
    size_t crashes = 0;
    for (const auto& seed : seeds) {
        for (int i = 0; i < 200; ++i) {
            std::string s = mutate(seed, rng);
            auto ast = parse_sql(s);
            (void)ast; // ensure no crash
        }
    }
    // If we got here, zero crashes
    printf("fuzz_smoke: ok\n");
    return crashes == 0 ? 0 : 1;
}
