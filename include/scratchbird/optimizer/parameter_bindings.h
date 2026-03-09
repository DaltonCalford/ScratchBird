#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::optimizer
{
    struct BoundParameterValue
    {
        bool is_null = false;
        std::string text;
    };

    struct ParameterBindings
    {
        std::vector<BoundParameterValue> positional;

        auto empty() const -> bool
        {
            return positional.empty();
        }

        auto getPositional(uint32_t one_based_index,
                           BoundParameterValue &value_out) const -> bool
        {
            if (one_based_index == 0)
            {
                return false;
            }

            const size_t index = static_cast<size_t>(one_based_index - 1);
            if (index >= positional.size())
            {
                return false;
            }

            value_out = positional[index];
            return true;
        }
    };

} // namespace scratchbird::optimizer
