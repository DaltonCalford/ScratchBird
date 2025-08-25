#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"
#include "scratchbird/engine/index_hash.h"

#include <iostream>
#include <memory>
#include <string>

using namespace scratchbird::engine;

int main()
{
    std::cout << "=== Hash Index Memory Debug Test ===" << std::endl;

    try {
        std::cout << "Creating FileMap..." << std::endl;

        // Create FileMap without TestDatabaseRAII to avoid heap issues
        FileMap::Layout layout;
        layout.page_size = 4096;
        layout.options.direct_io = false;
        FileMap fmap(layout);

        std::cout << "Creating HashIndex..." << std::endl;

        // Create hash index
        auto hash_index = std::make_unique<HashIndex>(std::move(fmap), 4096, false);

        std::cout << "Calling create_empty()..." << std::endl;
        hash_index->create_empty();

        std::cout << "Testing basic insertion..." << std::endl;
        std::string err;
        bool success = hash_index->insert("test_key", 123, err);

        if (success) {
            std::cout << "✓ Insert successful" << std::endl;
        } else {
            std::cout << "⚠ Insert failed: " << err << std::endl;
        }

        std::cout << "Testing search..." << std::endl;
        std::vector<std::uint64_t> results;
        hash_index->search_equal("test_key", results);

        if (!results.empty()) {
            std::cout << "✓ Search successful, found: " << results[0] << std::endl;
        } else {
            std::cout << "⚠ Search failed" << std::endl;
        }

        std::cout << "Destroying hash index..." << std::endl;
        hash_index.reset();

        std::cout << "✓ Memory debug test completed successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "⚠ Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
