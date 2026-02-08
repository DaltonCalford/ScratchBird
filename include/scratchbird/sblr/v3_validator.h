#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace scratchbird::sblr::v3 {

bool validateContainer(const uint8_t* data, size_t size, std::string& err);

}  // namespace scratchbird::sblr::v3
