#include "scratchbird/engine/index.h"

#include <cassert>
#include <string>
using namespace scratchbird::engine;

static bool has_error(const std::vector<ValidationMessage>& msgs)
{
    for (auto& m : msgs)
        if (m.error)
            return true;
    return false;
}

int main()
{
    // BTree ok
    IndexCreateOptions o{};
    o.index_name = "ix";
    o.relation_name = "t";
    o.method = IndexMethod::BTree;
    o.keys.push_back({"a", "", "ASC", ""});
    auto msgs = validate_index_definition(o);
    assert(!has_error(msgs));

    // Hash bad cases
    IndexCreateOptions h{};
    h.index_name = "ixh";
    h.relation_name = "t";
    h.method = IndexMethod::Hash;
    h.keys.push_back({"a", "", "", ""});
    h.keys.push_back({"b", "", "", ""});
    msgs = validate_index_definition(h);
    assert(has_error(msgs));

    IndexCreateOptions ph{};
    ph.index_name = "ixph";
    ph.relation_name = "t";
    ph.method = IndexMethod::PartialHash;
    ph.keys.push_back({"a", "", "", ""});
    msgs = validate_index_definition(ph);
    assert(has_error(msgs));
    ph.where_predicate = "a IS NOT NULL";
    msgs = validate_index_definition(ph);
    assert(!has_error(msgs));

    // Gin single key
    IndexCreateOptions gin{};
    gin.index_name = "ixg";
    gin.relation_name = "t";
    gin.method = IndexMethod::Gin;
    gin.keys.push_back({"vec", "", "", ""});
    msgs = validate_index_definition(gin);
    assert(!has_error(msgs));

    // Format
    auto s = format_index_definition(o);
    assert(s.find("INDEX ix ON t") != std::string::npos);
    return 0;
}
