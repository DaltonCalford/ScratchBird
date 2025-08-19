#include "scratchbird/engine/btree_page.h"
#include "scratchbird/engine/ods.h"

#include <cassert>
#include <string>
#include <vector>

using namespace scratchbird::engine;

static CompositeKey ck(std::initializer_list<KeyPart> parts)
{
    CompositeKey k{};
    k.parts.assign(parts.begin(), parts.end());
    return k;
}

int main()
{
    const std::uint32_t ps = 4096;

    // Leaf roundtrip
    {
        std::vector<std::uint8_t> page(ps, 0);
        auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
        ph->page_no = 123;
        std::vector<LeafRecordV1> recs;
        recs.push_back({ck({KeyPart{"a", false, false}, KeyPart{"", true, false}}), 1, "p1"});
        recs.push_back({ck({KeyPart{"b", false, true}}), 2, ""});
        CompositeKey high = ck({KeyPart{"c", false, false}});
        std::string prefix = "pre";
        build_leaf_page_v1(page, ps, recs, &high, &prefix);
        std::vector<LeafRecordV1> out;
        CompositeKey out_high;
        std::string out_pref;
        parse_leaf_page_v1(page, out, out_high, out_pref);
        assert(out.size() == recs.size());
        assert(out_pref == prefix);
        assert(out_high.parts.size() == 1 && out_high.parts[0].bytes == "c");
        assert(out[0].row_id == 1 && out[0].payload == "p1");
        assert(out[1].row_id == 2);
    }

    // Branch roundtrip
    {
        std::vector<std::uint8_t> page(ps, 0);
        auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
        ph->page_no = 200;
        std::vector<BranchEntryV1> ents;
        ents.push_back({ck({KeyPart{"ka", false, false}}), 10});
        ents.push_back({ck({KeyPart{"kb", false, false}, KeyPart{"x", false, true}}), 11});
        CompositeKey high = ck({KeyPart{"zz", false, false}});
        std::string prefix = "preB";
        build_branch_page_v1(page, ps, ents, /*leftmost_child*/ 42, &high, &prefix);
        std::vector<BranchEntryV1> out;
        CompositeKey out_high;
        std::string out_pref;
        std::uint32_t out_left = 0;
        parse_branch_page_v1(page, out, out_high, out_pref, out_left);
        assert(out.size() == ents.size());
        assert(out_pref == prefix);
        assert(out_high.parts.size() == 1 && out_high.parts[0].bytes == "zz");
        assert(out[0].child_page == 10);
        assert(out[1].child_page == 11);
        assert(out_left == 42);
    }

    return 0;
}
