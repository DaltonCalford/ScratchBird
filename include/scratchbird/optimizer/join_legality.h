#pragma once

#include "scratchbird/parser/shared_types.h"

#include <cstdint>
#include <string>

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
        bool natural_barrier = false;
        bool using_barrier = false;
        bool semi_duplicate_semantics = false;
        bool anti_duplicate_semantics = false;
        bool lateral_dependency = false;
    };

    enum class JoinMethodFamily : uint8_t
    {
        NESTED_LOOP = 0,
        PARAMETERIZED_NESTED_LOOP = 1,
        HASH_JOIN = 2,
        MERGE_JOIN = 3
    };

    enum class JoinMethodRejectCode : uint8_t
    {
        NONE = 0,
        OUTER_OR_SPECIAL_JOIN_REQUIRES_NESTED_LOOP = 1,
        NATURAL_JOIN_REQUIRES_NESTED_LOOP = 2,
        USING_JOIN_REQUIRES_NESTED_LOOP = 3,
        SEMI_JOIN_REQUIRES_NESTED_LOOP = 4,
        ANTI_JOIN_REQUIRES_NESTED_LOOP = 5,
        PARAMETERIZED_INNER_REQUIRES_NESTED_LOOP = 6,
        NON_EQUI_JOIN_REQUIRES_KEYS = 7,
        MISSING_KEY_METADATA = 8
    };

    struct JoinMethodLegality
    {
        JoinMethodFamily family = JoinMethodFamily::NESTED_LOOP;
        bool legal = false;
        bool requires_sort_outer = false;
        bool requires_sort_inner = false;
        bool parameterized_inner = false;
        JoinMethodRejectCode reject_code = JoinMethodRejectCode::NONE;
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
        descriptor.semi_duplicate_semantics = true;
        return descriptor;
    }

    inline auto makeAntiJoinLegality() -> JoinLegalityDescriptor
    {
        JoinLegalityDescriptor descriptor;
        descriptor.legality_class = JoinLegalityClass::ANTI_BARRIER;
        descriptor.reorderable = false;
        descriptor.requires_original_order = true;
        descriptor.anti_duplicate_semantics = true;
        return descriptor;
    }

    inline auto joinMethodFamilyName(JoinMethodFamily family) -> const char *
    {
        switch (family)
        {
            case JoinMethodFamily::NESTED_LOOP:
                return "NESTED_LOOP";
            case JoinMethodFamily::PARAMETERIZED_NESTED_LOOP:
                return "PARAMETERIZED_NESTED_LOOP";
            case JoinMethodFamily::HASH_JOIN:
                return "HASH_JOIN";
            case JoinMethodFamily::MERGE_JOIN:
                return "MERGE_JOIN";
        }
        return "NESTED_LOOP";
    }

    inline auto joinMethodRejectCodeName(JoinMethodRejectCode code) -> const char *
    {
        switch (code)
        {
            case JoinMethodRejectCode::NONE:
                return "NONE";
            case JoinMethodRejectCode::OUTER_OR_SPECIAL_JOIN_REQUIRES_NESTED_LOOP:
                return "JOIN_METHOD_SPECIAL_JOIN_TYPE";
            case JoinMethodRejectCode::NATURAL_JOIN_REQUIRES_NESTED_LOOP:
                return "JOIN_METHOD_NATURAL_BARRIER";
            case JoinMethodRejectCode::USING_JOIN_REQUIRES_NESTED_LOOP:
                return "JOIN_METHOD_USING_BARRIER";
            case JoinMethodRejectCode::SEMI_JOIN_REQUIRES_NESTED_LOOP:
                return "JOIN_METHOD_SEMI_BARRIER";
            case JoinMethodRejectCode::ANTI_JOIN_REQUIRES_NESTED_LOOP:
                return "JOIN_METHOD_ANTI_BARRIER";
            case JoinMethodRejectCode::PARAMETERIZED_INNER_REQUIRES_NESTED_LOOP:
                return "JOIN_METHOD_PARAMETERIZED_INNER";
            case JoinMethodRejectCode::NON_EQUI_JOIN_REQUIRES_KEYS:
                return "JOIN_METHOD_NON_EQUI";
            case JoinMethodRejectCode::MISSING_KEY_METADATA:
                return "JOIN_METHOD_MISSING_KEY_METADATA";
        }
        return "NONE";
    }

    inline auto joinMethodRejectReason(JoinMethodFamily family,
                                       JoinMethodRejectCode code) -> std::string
    {
        if (code == JoinMethodRejectCode::NONE)
        {
            return {};
        }

        const std::string prefix =
            std::string(joinMethodRejectCodeName(code)) + ": ";
        switch (code)
        {
            case JoinMethodRejectCode::OUTER_OR_SPECIAL_JOIN_REQUIRES_NESTED_LOOP:
                return prefix + std::string(joinMethodFamilyName(family)) +
                    " is not legal for this join type";
            case JoinMethodRejectCode::NATURAL_JOIN_REQUIRES_NESTED_LOOP:
                return prefix + "NATURAL join requires nested loop";
            case JoinMethodRejectCode::USING_JOIN_REQUIRES_NESTED_LOOP:
                return prefix + "USING join requires nested loop";
            case JoinMethodRejectCode::SEMI_JOIN_REQUIRES_NESTED_LOOP:
                return prefix + "semi-join duplication semantics require nested loop";
            case JoinMethodRejectCode::ANTI_JOIN_REQUIRES_NESTED_LOOP:
                return prefix + "anti-join duplication semantics require nested loop";
            case JoinMethodRejectCode::PARAMETERIZED_INNER_REQUIRES_NESTED_LOOP:
                return prefix + "parameterized inner path requires nested loop";
            case JoinMethodRejectCode::NON_EQUI_JOIN_REQUIRES_KEYS:
                return prefix + std::string(joinMethodFamilyName(family)) +
                    " requires equi-join keys";
            case JoinMethodRejectCode::MISSING_KEY_METADATA:
                return prefix + std::string(joinMethodFamilyName(family)) +
                    " requires resolved join key metadata";
            case JoinMethodRejectCode::NONE:
                break;
        }
        return {};
    }

    inline auto makeJoinMethodDescriptor(const JoinLegalityDescriptor &base,
                                         bool parameterized_inner)
        -> JoinLegalityDescriptor
    {
        auto descriptor = base;
        descriptor.lateral_dependency = descriptor.lateral_dependency || parameterized_inner;
        return descriptor;
    }

    inline auto evaluateNestedLoopLegality(const JoinLegalityDescriptor &descriptor,
                                           bool parameterized_inner)
        -> JoinMethodLegality
    {
        (void)descriptor;
        JoinMethodLegality legality;
        legality.family = parameterized_inner
            ? JoinMethodFamily::PARAMETERIZED_NESTED_LOOP
            : JoinMethodFamily::NESTED_LOOP;
        legality.legal = true;
        legality.parameterized_inner = parameterized_inner;
        return legality;
    }

    inline auto evaluateHashJoinLegality(const JoinLegalityDescriptor &descriptor,
                                         parser::JoinType join_type,
                                         bool equi_join,
                                         bool has_key_metadata,
                                         bool parameterized_inner)
        -> JoinMethodLegality
    {
        JoinMethodLegality legality;
        legality.family = JoinMethodFamily::HASH_JOIN;
        legality.parameterized_inner = parameterized_inner;

        if (descriptor.natural_barrier)
        {
            legality.reject_code =
                JoinMethodRejectCode::NATURAL_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (descriptor.using_barrier)
        {
            legality.reject_code =
                JoinMethodRejectCode::USING_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (descriptor.semi_duplicate_semantics)
        {
            legality.reject_code =
                JoinMethodRejectCode::SEMI_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (descriptor.anti_duplicate_semantics)
        {
            legality.reject_code =
                JoinMethodRejectCode::ANTI_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (descriptor.lateral_dependency || parameterized_inner)
        {
            legality.reject_code =
                JoinMethodRejectCode::PARAMETERIZED_INNER_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (join_type != parser::JoinType::INNER &&
            join_type != parser::JoinType::LEFT)
        {
            legality.reject_code =
                JoinMethodRejectCode::OUTER_OR_SPECIAL_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (!equi_join)
        {
            legality.reject_code =
                JoinMethodRejectCode::NON_EQUI_JOIN_REQUIRES_KEYS;
            return legality;
        }
        if (!has_key_metadata)
        {
            legality.reject_code = JoinMethodRejectCode::MISSING_KEY_METADATA;
            return legality;
        }

        legality.legal = true;
        return legality;
    }

    inline auto evaluateMergeJoinLegality(const JoinLegalityDescriptor &descriptor,
                                          parser::JoinType join_type,
                                          bool equi_join,
                                          bool has_key_metadata,
                                          bool parameterized_inner,
                                          bool outer_presorted,
                                          bool inner_presorted)
        -> JoinMethodLegality
    {
        JoinMethodLegality legality;
        legality.family = JoinMethodFamily::MERGE_JOIN;
        legality.parameterized_inner = parameterized_inner;

        if (descriptor.natural_barrier)
        {
            legality.reject_code =
                JoinMethodRejectCode::NATURAL_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (descriptor.using_barrier)
        {
            legality.reject_code =
                JoinMethodRejectCode::USING_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (descriptor.semi_duplicate_semantics)
        {
            legality.reject_code =
                JoinMethodRejectCode::SEMI_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (descriptor.anti_duplicate_semantics)
        {
            legality.reject_code =
                JoinMethodRejectCode::ANTI_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (descriptor.lateral_dependency || parameterized_inner)
        {
            legality.reject_code =
                JoinMethodRejectCode::PARAMETERIZED_INNER_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (join_type != parser::JoinType::INNER &&
            join_type != parser::JoinType::LEFT)
        {
            legality.reject_code =
                JoinMethodRejectCode::OUTER_OR_SPECIAL_JOIN_REQUIRES_NESTED_LOOP;
            return legality;
        }
        if (!equi_join)
        {
            legality.reject_code =
                JoinMethodRejectCode::NON_EQUI_JOIN_REQUIRES_KEYS;
            return legality;
        }
        if (!has_key_metadata)
        {
            legality.reject_code = JoinMethodRejectCode::MISSING_KEY_METADATA;
            return legality;
        }

        legality.legal = true;
        legality.requires_sort_outer = !outer_presorted;
        legality.requires_sort_inner = !inner_presorted;
        return legality;
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
                descriptor.natural_barrier = true;
                break;
            case parser::JoinType::NATURAL_LEFT:
                natural = true;
                descriptor.legality_class = JoinLegalityClass::NATURAL_BARRIER;
                descriptor.reorderable = false;
                descriptor.preserves_left_rows = true;
                descriptor.null_introduces_right = true;
                descriptor.requires_original_order = true;
                descriptor.natural_barrier = true;
                break;
            case parser::JoinType::NATURAL_RIGHT:
                natural = true;
                descriptor.legality_class = JoinLegalityClass::NATURAL_BARRIER;
                descriptor.reorderable = false;
                descriptor.preserves_right_rows = true;
                descriptor.null_introduces_left = true;
                descriptor.requires_original_order = true;
                descriptor.natural_barrier = true;
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
                descriptor.natural_barrier = true;
                break;
        }

        if (natural)
        {
            descriptor.legality_class = JoinLegalityClass::NATURAL_BARRIER;
            descriptor.reorderable = false;
            descriptor.requires_original_order = true;
            descriptor.natural_barrier = true;
            return descriptor;
        }

        if (has_using)
        {
            if (descriptor.reorderable)
            {
                descriptor.legality_class = JoinLegalityClass::USING_BARRIER;
                descriptor.reorderable = false;
                descriptor.requires_original_order = true;
            }
            descriptor.using_barrier = true;
        }

        return descriptor;
    }

} // namespace scratchbird::optimizer
