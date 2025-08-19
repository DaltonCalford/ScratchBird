#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace scratchbird::engine::ods;

int main()
{
    std::vector<std::uint8_t> page(4096, 0);
    auto* ph = reinterpret_cast<PageHeader*>(page.data());
    ph->page_size = 4096;
    ph->type = static_cast<std::uint16_t>(PageType::HeapRoot);
    HeapRootPayload hr{};
    hr.version = 1;
    hr.first_heap_page = 123;
    hr.last_heap_page = 123;
    std::memcpy(page.data() + sizeof(PageHeader), &hr, sizeof(hr));

    HeapRootPayload check{};
    std::memcpy(&check, page.data() + sizeof(PageHeader), sizeof(check));
    if (check.first_heap_page != 123 || check.last_heap_page != 123) {
        std::cerr << "heap_root encode/decode mismatch" << std::endl;
        return 1;
    }
    std::cout << "heap_root_tests ok" << std::endl;
    return 0;
}
