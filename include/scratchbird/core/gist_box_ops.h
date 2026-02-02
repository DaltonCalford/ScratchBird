/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/gist_index.h"
#include <cmath>
#include <algorithm>

namespace scratchbird::core
{

/**
 * Geometric box structure
 */
struct Box
{
    double min_x;
    double min_y;
    double max_x;
    double max_y;

    Box() : min_x(0), min_y(0), max_x(0), max_y(0) {}
    Box(double x1, double y1, double x2, double y2)
        : min_x(std::min(x1, x2))
        , min_y(std::min(y1, y2))
        , max_x(std::max(x1, x2))
        , max_y(std::max(y1, y2))
    {}

    double area() const
    {
        return (max_x - min_x) * (max_y - min_y);
    }

    bool overlaps(const Box& other) const
    {
        return !(max_x < other.min_x || min_x > other.max_x ||
                max_y < other.min_y || min_y > other.max_y);
    }

    bool contains(const Box& other) const
    {
        return min_x <= other.min_x && max_x >= other.max_x &&
               min_y <= other.min_y && max_y >= other.max_y;
    }

    Box unionWith(const Box& other) const
    {
        return Box(std::min(min_x, other.min_x),
                  std::min(min_y, other.min_y),
                  std::max(max_x, other.max_x),
                  std::max(max_y, other.max_y));
    }

    double distanceTo(const Box& other) const
    {
        double dx = 0.0;
        double dy = 0.0;

        if (max_x < other.min_x)
            dx = other.min_x - max_x;
        else if (min_x > other.max_x)
            dx = min_x - other.max_x;

        if (max_y < other.min_y)
            dy = other.min_y - max_y;
        else if (min_y > other.max_y)
            dy = min_y - other.max_y;

        return std::sqrt(dx * dx + dy * dy);
    }

    std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> result(sizeof(Box));
        std::memcpy(result.data(), this, sizeof(Box));
        return result;
    }

    static Box deserialize(const std::vector<uint8_t>& data)
    {
        if (data.size() < sizeof(Box))
            return Box();

        Box box;
        std::memcpy(&box, data.data(), sizeof(Box));
        return box;
    }
};

/**
 * GiST operator class for geometric boxes (box_ops)
 *
 * Implements R-Tree semantics for 2D bounding boxes.
 */
class GiSTBoxOperatorClass : public GiSTOperatorClass
{
public:
    static constexpr uint32_t OPCLASS_ID = 1;
    static constexpr const char* OPCLASS_NAME = "box_ops";

    uint32_t getOpClassId() const override { return OPCLASS_ID; }
    std::string getOpClassName() const override { return OPCLASS_NAME; }

    bool consistent(const GiSTPredicate& predicate,
                   const std::vector<uint8_t>& query,
                   GiSTStrategy strategy) const override
    {
        Box pred_box = Box::deserialize(predicate.data);
        Box query_box = Box::deserialize(query);

        switch (strategy)
        {
            case GiSTStrategy::OVERLAPS:
                return pred_box.overlaps(query_box);

            case GiSTStrategy::CONTAINS:
                return pred_box.contains(query_box);

            case GiSTStrategy::CONTAINED_BY:
                return query_box.contains(pred_box);

            case GiSTStrategy::LEFT_OF:
                return pred_box.max_x < query_box.min_x;

            case GiSTStrategy::RIGHT_OF:
                return pred_box.min_x > query_box.max_x;

            case GiSTStrategy::BELOW:
                return pred_box.max_y < query_box.min_y;

            case GiSTStrategy::ABOVE:
                return pred_box.min_y > query_box.max_y;

            case GiSTStrategy::EQUALS:
                return pred_box.min_x == query_box.min_x &&
                       pred_box.min_y == query_box.min_y &&
                       pred_box.max_x == query_box.max_x &&
                       pred_box.max_y == query_box.max_y;

            default:
                return false;
        }
    }

    GiSTPredicate unionPredicates(
        const std::vector<GiSTPredicate>& entries) const override
    {
        if (entries.empty())
            return GiSTPredicate({}, OPCLASS_ID);

        Box result = Box::deserialize(entries[0].data);
        for (size_t i = 1; i < entries.size(); ++i)
        {
            Box box = Box::deserialize(entries[i].data);
            result = result.unionWith(box);
        }

        return GiSTPredicate(result.serialize(), OPCLASS_ID);
    }

    double penalty(const GiSTPredicate& base,
                  const GiSTPredicate& add) const override
    {
        Box base_box = Box::deserialize(base.data);
        Box add_box = Box::deserialize(add.data);
        Box union_box = base_box.unionWith(add_box);

        // Penalty is the area increase
        return union_box.area() - base_box.area();
    }

    void picksplit(const std::vector<GiSTPredicate>& entries,
                  std::vector<size_t>& left_indices,
                  std::vector<size_t>& right_indices) const override
    {
        if (entries.size() < 2)
        {
            left_indices.push_back(0);
            return;
        }

        // Quadratic split algorithm
        // 1. Find the pair with maximum wasted area (PickSeeds)
        double max_waste = -1.0;
        size_t seed1 = 0, seed2 = 1;

        for (size_t i = 0; i < entries.size(); ++i)
        {
            Box box_i = Box::deserialize(entries[i].data);
            for (size_t j = i + 1; j < entries.size(); ++j)
            {
                Box box_j = Box::deserialize(entries[j].data);
                Box union_box = box_i.unionWith(box_j);
                double waste = union_box.area() - box_i.area() - box_j.area();

                if (waste > max_waste)
                {
                    max_waste = waste;
                    seed1 = i;
                    seed2 = j;
                }
            }
        }

        // 2. Initialize groups with seeds
        left_indices.push_back(seed1);
        right_indices.push_back(seed2);

        Box left_box = Box::deserialize(entries[seed1].data);
        Box right_box = Box::deserialize(entries[seed2].data);

        // 3. Assign remaining entries (PickNext)
        std::vector<bool> assigned(entries.size(), false);
        assigned[seed1] = true;
        assigned[seed2] = true;

        for (size_t remaining = entries.size() - 2; remaining > 0; --remaining)
        {
            double max_diff = -1.0;
            size_t best_entry = 0;
            bool prefer_left = true;

            // Find entry with maximum preference difference
            for (size_t i = 0; i < entries.size(); ++i)
            {
                if (assigned[i])
                    continue;

                Box box = Box::deserialize(entries[i].data);
                double left_enlargement = left_box.unionWith(box).area() - left_box.area();
                double right_enlargement = right_box.unionWith(box).area() - right_box.area();
                double diff = std::abs(left_enlargement - right_enlargement);

                if (diff > max_diff)
                {
                    max_diff = diff;
                    best_entry = i;
                    prefer_left = (left_enlargement < right_enlargement);
                }
            }

            // Assign to preferred group
            Box box = Box::deserialize(entries[best_entry].data);
            if (prefer_left)
            {
                left_indices.push_back(best_entry);
                left_box = left_box.unionWith(box);
            }
            else
            {
                right_indices.push_back(best_entry);
                right_box = right_box.unionWith(box);
            }
            assigned[best_entry] = true;
        }
    }

    bool same(const GiSTPredicate& a,
             const GiSTPredicate& b) const override
    {
        Box box_a = Box::deserialize(a.data);
        Box box_b = Box::deserialize(b.data);

        return box_a.min_x == box_b.min_x &&
               box_a.min_y == box_b.min_y &&
               box_a.max_x == box_b.max_x &&
               box_a.max_y == box_b.max_y;
    }

    double distance(const GiSTPredicate& predicate,
                   const std::vector<uint8_t>& query) const override
    {
        Box pred_box = Box::deserialize(predicate.data);
        Box query_box = Box::deserialize(query);
        return pred_box.distanceTo(query_box);
    }
};

} // namespace scratchbird::core
