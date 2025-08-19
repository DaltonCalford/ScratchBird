#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/header.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace scratchbird::engine;

static void print_uuid(const UuidBytes& u)
{
    for (size_t i = 0; i < u.size(); ++i) {
        std::printf("%02x", static_cast<unsigned>(u[i]));
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: catalog_inspect <db_base_path>\n");
        return 2;
    }
    std::string db = argv[1];
    CatalogManager cm(db);
    auto ver = cm.current_version();
    std::printf("Catalog version: %u.%u\n", static_cast<unsigned>(ver.major),
                static_cast<unsigned>(ver.minor));

    auto schemas = cm.list_schemas();
    std::printf("Schemas (%zu):\n", schemas.size());
    for (const auto& [oid, name] : schemas) {
        std::printf("  ");
        print_uuid(oid);
        std::printf("  %s\n", name.c_str());
    }
    return 0;
}
