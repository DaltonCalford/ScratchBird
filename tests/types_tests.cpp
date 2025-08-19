#include "scratchbird/engine/ods.h"
#include "scratchbird/engine/types.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        using namespace scratchbird::engine::ods;
        // Sanity across page sizes, no overflow
        const std::uint32_t sizes[] = {4096u, 8192u, 16384u, 32768u, 65536u, 131072u};
        for (auto ps : sizes) {
            auto b = bytesBitPIP(ps);
            auto p = pagesPerPIP(ps);
            auto t = transPerTIP(ps);
            auto g = gensPerPage(ps);
            (void)b;
            (void)p;
            (void)t;
            (void)g; // ensure link
        }
    }
    auto td = parse_type_spec("NUMERIC(10,2)");
    assert(td.kind == TypeKind::Numeric && td.precision == 10 && td.scale == 2);

    td = parse_type_spec("VARCHAR(255)");
    assert(td.kind == TypeKind::VarChar && td.length == 255);

    td = parse_type_spec("DOUBLE PRECISION");
    assert(td.kind == TypeKind::DoublePrecision);

    td = parse_type_spec("VECTOR(128)");
    assert(td.kind == TypeKind::Vector && td.vector_dims == 128);

    td = parse_type_spec("DATE");
    assert(td.kind == TypeKind::Date);

    return 0;
}
