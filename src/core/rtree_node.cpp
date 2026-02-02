/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/rtree_node.h"
#include <algorithm>
#include <limits>
#include <cmath>

namespace scratchbird::core
{

// ============================================================================
// BoundingBox Implementation
// ============================================================================

double BoundingBox::area() const
{
    if (!isValid())
        return 0.0;

    double width = max_x - min_x;
    double height = max_y - min_y;
    return width * height;
}

double BoundingBox::perimeter() const
{
    if (!isValid())
        return 0.0;

    double width = max_x - min_x;
    double height = max_y - min_y;
    return 2.0 * (width + height);
}

bool BoundingBox::contains(const BoundingBox& other) const
{
    return (min_x <= other.min_x && max_x >= other.max_x &&
            min_y <= other.min_y && max_y >= other.max_y);
}

bool BoundingBox::intersects(const BoundingBox& other) const
{
    return !(max_x < other.min_x || min_x > other.max_x ||
             max_y < other.min_y || min_y > other.max_y);
}

double BoundingBox::enlargement(const BoundingBox& other) const
{
    BoundingBox merged = merge(*this, other);
    return merged.area() - this->area();
}

double BoundingBox::overlap(const BoundingBox& other) const
{
    if (!intersects(other))
        return 0.0;

    double x_overlap = std::min(max_x, other.max_x) - std::max(min_x, other.min_x);
    double y_overlap = std::min(max_y, other.max_y) - std::max(min_y, other.min_y);

    return x_overlap * y_overlap;
}

void BoundingBox::expand(const BoundingBox& other)
{
    min_x = std::min(min_x, other.min_x);
    min_y = std::min(min_y, other.min_y);
    max_x = std::max(max_x, other.max_x);
    max_y = std::max(max_y, other.max_y);
}

BoundingBox BoundingBox::merge(const BoundingBox& a, const BoundingBox& b)
{
    return BoundingBox(
        std::min(a.min_x, b.min_x),
        std::min(a.min_y, b.min_y),
        std::max(a.max_x, b.max_x),
        std::max(a.max_y, b.max_y)
    );
}

bool BoundingBox::isValid() const
{
    return (min_x <= max_x && min_y <= max_y);
}

void BoundingBox::getCenter(double* x, double* y) const
{
    *x = (min_x + max_x) / 2.0;
    *y = (min_y + max_y) / 2.0;
}

// ============================================================================
// RTreeNode Implementation
// ============================================================================

RTreeNode::RTreeNode(bool is_leaf, uint32_t max_entries)
    : is_leaf_(is_leaf)
    , max_entries_(max_entries)
    , parent_(nullptr)
    , page_number_(0)
    , level_(0)
    , xmin_(0)
    , xmax_(0)
{
    entries_.reserve(max_entries);
}

RTreeNode::~RTreeNode()
{
    // Nothing to do - using STL containers
}

void RTreeNode::addEntry(const RTreeEntry& entry)
{
    entries_.push_back(entry);
}

void RTreeNode::removeEntry(size_t index)
{
    if (index < entries_.size())
    {
        entries_.erase(entries_.begin() + index);
    }
}

bool RTreeNode::hasMinimumEntries() const
{
    // Minimum entries is typically M/2 (rounded up)
    // For root, minimum is 2 (unless it's a leaf)
    if (isRoot())
    {
        return is_leaf_ ? true : entries_.size() >= 2;
    }

    uint32_t min_entries = (max_entries_ + 1) / 2;
    return entries_.size() >= min_entries;
}

BoundingBox RTreeNode::calculateMBR() const
{
    if (entries_.empty())
    {
        return BoundingBox();
    }

    BoundingBox mbr = entries_[0].bbox;

    for (size_t i = 1; i < entries_.size(); ++i)
    {
        mbr.expand(entries_[i].bbox);
    }

    return mbr;
}

size_t RTreeNode::findMinEnlargement(const BoundingBox& bbox) const
{
    if (entries_.empty())
        return 0;

    size_t best_index = 0;
    double min_enlargement = std::numeric_limits<double>::max();
    double min_area = std::numeric_limits<double>::max();

    for (size_t i = 0; i < entries_.size(); ++i)
    {
        double enlargement = entries_[i].bbox.enlargement(bbox);
        double area = entries_[i].bbox.area();

        // Use enlargement as primary criterion
        // Tie-break by smallest area (R*-tree optimization)
        if (enlargement < min_enlargement ||
            (enlargement == min_enlargement && area < min_area))
        {
            min_enlargement = enlargement;
            min_area = area;
            best_index = i;
        }
    }

    return best_index;
}

size_t RTreeNode::findMinOverlap(const BoundingBox& bbox) const
{
    if (entries_.empty())
        return 0;

    size_t best_index = 0;
    double min_overlap_increase = std::numeric_limits<double>::max();
    double min_enlargement = std::numeric_limits<double>::max();

    for (size_t i = 0; i < entries_.size(); ++i)
    {
        // Calculate current overlap with all other entries
        double current_overlap = 0.0;
        for (size_t j = 0; j < entries_.size(); ++j)
        {
            if (i != j)
            {
                current_overlap += entries_[i].bbox.overlap(entries_[j].bbox);
            }
        }

        // Calculate overlap after expanding to include bbox
        BoundingBox expanded = BoundingBox::merge(entries_[i].bbox, bbox);
        double new_overlap = 0.0;
        for (size_t j = 0; j < entries_.size(); ++j)
        {
            if (i != j)
            {
                new_overlap += expanded.overlap(entries_[j].bbox);
            }
        }

        double overlap_increase = new_overlap - current_overlap;
        double enlargement = entries_[i].bbox.enlargement(bbox);

        // Use overlap increase as primary criterion
        // Tie-break by enlargement (R*-tree optimization)
        if (overlap_increase < min_overlap_increase ||
            (overlap_increase == min_overlap_increase && enlargement < min_enlargement))
        {
            min_overlap_increase = overlap_increase;
            min_enlargement = enlargement;
            best_index = i;
        }
    }

    return best_index;
}

std::unique_ptr<RTreeNode> RTreeNode::split()
{
    // Create new sibling node
    auto new_node = std::make_unique<RTreeNode>(is_leaf_, max_entries_);
    new_node->setLevel(level_);
    new_node->setParent(parent_);
    new_node->setXmin(xmin_);

    std::vector<RTreeEntry> group1, group2;

    // Use R*-tree quadratic split algorithm
    quadraticSplit(group1, group2);

    // Assign entries to this node and new sibling
    entries_ = std::move(group1);
    new_node->entries_ = std::move(group2);

    return new_node;
}

void RTreeNode::quadraticSplit(std::vector<RTreeEntry>& group1,
                              std::vector<RTreeEntry>& group2)
{
    // R*-tree quadratic split algorithm
    // 1. Pick two seeds (entries that would waste most area if grouped)
    // 2. Assign remaining entries to the group requiring least enlargement

    size_t seed1, seed2;
    pickSeeds(&seed1, &seed2);

    // Initialize groups with seeds
    group1.push_back(entries_[seed1]);
    group2.push_back(entries_[seed2]);

    // Calculate initial MBRs
    BoundingBox mbr1 = entries_[seed1].bbox;
    BoundingBox mbr2 = entries_[seed2].bbox;

    // Track which entries have been assigned
    std::vector<bool> assigned(entries_.size(), false);
    assigned[seed1] = true;
    assigned[seed2] = true;

    uint32_t min_entries = (max_entries_ + 1) / 2;

    // Assign remaining entries
    while (true)
    {
        // Check if all entries are assigned
        size_t unassigned = 0;
        for (bool a : assigned)
        {
            if (!a)
                unassigned++;
        }

        if (unassigned == 0)
            break;

        // If one group needs all remaining entries to reach minimum
        if (group1.size() + unassigned == min_entries)
        {
            // Assign all remaining to group1
            for (size_t i = 0; i < entries_.size(); ++i)
            {
                if (!assigned[i])
                {
                    group1.push_back(entries_[i]);
                    mbr1.expand(entries_[i].bbox);
                    assigned[i] = true;
                }
            }
            break;
        }

        if (group2.size() + unassigned == min_entries)
        {
            // Assign all remaining to group2
            for (size_t i = 0; i < entries_.size(); ++i)
            {
                if (!assigned[i])
                {
                    group2.push_back(entries_[i]);
                    mbr2.expand(entries_[i].bbox);
                    assigned[i] = true;
                }
            }
            break;
        }

        // Pick next entry to assign
        size_t next = pickNext(mbr1, mbr2, assigned);
        assigned[next] = true;

        // Assign to group with least enlargement
        double enlargement1 = mbr1.enlargement(entries_[next].bbox);
        double enlargement2 = mbr2.enlargement(entries_[next].bbox);

        if (enlargement1 < enlargement2)
        {
            group1.push_back(entries_[next]);
            mbr1.expand(entries_[next].bbox);
        }
        else if (enlargement2 < enlargement1)
        {
            group2.push_back(entries_[next]);
            mbr2.expand(entries_[next].bbox);
        }
        else
        {
            // Tie - assign to group with smaller area
            double area1 = mbr1.area();
            double area2 = mbr2.area();

            if (area1 < area2)
            {
                group1.push_back(entries_[next]);
                mbr1.expand(entries_[next].bbox);
            }
            else if (area2 < area1)
            {
                group2.push_back(entries_[next]);
                mbr2.expand(entries_[next].bbox);
            }
            else
            {
                // Still tied - assign to smaller group
                if (group1.size() <= group2.size())
                {
                    group1.push_back(entries_[next]);
                    mbr1.expand(entries_[next].bbox);
                }
                else
                {
                    group2.push_back(entries_[next]);
                    mbr2.expand(entries_[next].bbox);
                }
            }
        }
    }
}

void RTreeNode::pickSeeds(size_t* seed1, size_t* seed2) const
{
    // Pick the pair of entries that would waste most area if grouped together
    // This is measured by: area(MBR(E1,E2)) - area(E1) - area(E2)

    double max_waste = -1.0;
    *seed1 = 0;
    *seed2 = 1;

    for (size_t i = 0; i < entries_.size(); ++i)
    {
        for (size_t j = i + 1; j < entries_.size(); ++j)
        {
            BoundingBox merged = BoundingBox::merge(entries_[i].bbox, entries_[j].bbox);
            double waste = merged.area() - entries_[i].bbox.area() - entries_[j].bbox.area();

            if (waste > max_waste)
            {
                max_waste = waste;
                *seed1 = i;
                *seed2 = j;
            }
        }
    }
}

size_t RTreeNode::pickNext(const BoundingBox& mbr1, const BoundingBox& mbr2,
                          const std::vector<bool>& assigned) const
{
    // Pick the entry with maximum preference for one group
    // Preference = |enlargement(group1) - enlargement(group2)|

    size_t best_index = 0;
    double max_diff = -1.0;

    for (size_t i = 0; i < entries_.size(); ++i)
    {
        if (assigned[i])
            continue;

        double enlargement1 = mbr1.enlargement(entries_[i].bbox);
        double enlargement2 = mbr2.enlargement(entries_[i].bbox);
        double diff = std::abs(enlargement1 - enlargement2);

        if (diff > max_diff)
        {
            max_diff = diff;
            best_index = i;
        }
    }

    return best_index;
}

} // namespace scratchbird::core
