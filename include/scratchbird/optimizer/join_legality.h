#pragma once

#include "scratchbird/parser/shared_types.h"

#include <cstdint>

namespace scratchbird::optimizer
{

    enum class JoinLegalityClass : uint8_t
    {
        INNER_REORDERABLE = 0,
        CROSS_REORDERABLE = 1,
        LEFT_OUTER_BARRIER = 2,
        RIGHT_OUTER_BARRIER = 3,
        FULL_OUTER_BARRIER = 4,
        NATURAL_BARRIER = 5,
        USING_BARRIER = 6,
        SEMI_BARRIER = 7,
        ANTI_BARRIER = 8
    };

    struct JoinLegalityDescriptor
    {
        JoinLegalityClass legality_class = JoinLegalityClass::INNER_REORDERABLE;
        bool reorderable = true;
        bool preserves_left_rows = false;
        bool preserves_right_rows = false;
        bool null_introduces_left = false;
        bool null_introduces_right = false;
        bool requires_original_order = false;
    };

    inline auto joinLegalityClassName(JoinLegalityClass legality_class) -> const char *
    {
        switch (legality_class)
        {
            case JoinLegalityClass::INNER_REORDERABLE:
                return "INNER_REORDERABLE";
            case JoinLegalityClass::CROSS_REORDERABLE:
                return "CROSS_REORDERABLE";
            case JoinLegalityClass::LEFT_OUTER_BARRIER:
                return "LEFT_OUTER_BARRIER";
            case JoinLegalityClass::RIGHT_OUTER_BARRIER:
                return "RIGHT_OUTER_BARRIER";
            case JoinLegalityClass::FULL_OUTER_BARRIER:
                return "FULL_OUTER_BARRIER";
            case JoinLegalityClass::NATURAL_BARRIER:
                return "NATURAL_BARRIER";
            case JoinLegalityClass::USING_BARRIER:
                return "USING_BARRIER";
            case JoinLegalityClass::SEMI_BARRIER:
                return "SEMI_BARRIER";
            case JoinLegalityClass::ANTI_BARRIER:
                return "ANTI_BARRIER";
        }
        return "INNER_REORDERABLE";
    }

    inline auto makeSemiJoinLegality() -> JoinLegalityDescriptor
    {
        JoinLegalityDescriptor descriptor;
        descriptor.legality_class = JoinLegalityClass::SEMI_BARRIER;
        descriptor.reorderable = false;
        descriptor.requires_original_order = true;
        return descriptor;
    }

    inline auto makeAntiJoinLegality() -> JoinLegalityDescriptor
    {
        JoinLegalityDescriptor descriptor;
        descriptor.legality_class = JoinLegalityClass::ANTI_BARRIER;
        descriptor.reorderable = false;
        descriptor.requires_original_order = true;
        return descriptor;
    }

    inline auto classifyJoinLegality(parser::JoinType join_type,
                                     bool natural,
                                     bool has_using) -> JoinLegalityDescriptor
    {
        JoinLegalityDescriptor descriptor;

        switch (join_type)
        {
            case parser::JoinType::INNER:
                descriptor.legality_class = JoinLegalityClass::INNER_REORDERABLE;
                break;
            case parser::JoinType::CROSS:
                descriptor.legality_class = JoinLegalityClass::CROSS_REORDERABLE;
                break;
            case parser::JoinType::LEFT:
                descriptor.legality_class = JoinLegalityClass::LEFT_OUTER_BARRIER;
                descriptor.reorderable = false;
                descriptor.preserves_left_rows = true;
                descriptor.null_introduces_right = true;
                descriptor.requires_original_order = true;
                break;
            case parser::JoinType::RIGHT:
                descriptor.legality_class = JoinLegalityClass::RIGHT_OUTER_BARRIER;
                descriptor.reorderable = false;
                descriptor.preserves_right_rows = true;
                descriptor.null_introduces_left = true;
                descriptor.requires_original_order = true;
                break;
            case parser::JoinType::FULL:
                descriptor.legality_class = JoinLegalityClass::FULL_OUTER_BARRIER;
                descriptor.reorderable = false;
                descriptor.preserves_left_rows = true;
                descriptor.preserves_right_rows = true;
                descriptor.null_introduces_left = true;
                descriptor.null_introduces_right = true;
                descriptor.requires_original_order = true;
                break;
            case parser::JoinType::NATURAL:
                natural = true;
                descriptor.legality_class = JoinLegalityClass::NATURAL_BARRIER;
                descriptor.reorderable = false;
                descriptor.requires_original_order = true;
                break;
            case parser::JoinType::NATURAL_LEFT:
                natural = true;
                descriptor.legality_class = JoinLegalityClass::NATURAL_BARRIER;
                descriptor.reorderable = false;
                descriptor.preserves_left_rows = true;
                descriptor.null_introduces_right = true;
                descriptor.requires_original_order = true;
                break;
            case parser::JoinType::NATURAL_RIGHT:
                natural = true;
                descriptor.legality_class = JoinLegalityClass::NATURAL_BARRIER;
                descriptor.reorderable = false;
                descriptor.preserves_right_rows = true;
                descriptor.null_introduces_left = true;
                descriptor.requires_original_order = true;
                break;
            case parser::JoinType::NATURAL_FULL:
                natural = true;
                descriptor.legality_class = JoinLegalityClass::NATURAL_BARRIER;
                descriptor.reorderable = false;
                descriptor.preserves_left_rows = true;
                descriptor.preserves_right_rows = true;
                descriptor.null_introduces_left = true;
                descriptor.null_introduces_right = true;
                descriptor.requires_original_order = true;
                break;
        }

        if (natural)
        {
            descriptor.legality_class = JoinLegalityClass::NATURAL_BARRIER;
            descriptor.reorderable = false;
            descriptor.requires_original_order = true;
            return descriptor;
        }

        if (has_using && descriptor.reorderable)
        {
            descriptor.legality_class = JoinLegalityClass::USING_BARRIER;
            descriptor.reorderable = false;
            descriptor.requires_original_order = true;
        }

        return descriptor;
    }

} // namespace scratchbird::optimizer
