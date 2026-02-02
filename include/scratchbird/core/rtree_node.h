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

#include "scratchbird/core/tid.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace scratchbird::core
{

/**
 * BoundingBox - Minimum Bounding Rectangle (MBR) for 2D spatial data
 *
 * Represents a rectangle in 2D space using min/max coordinates.
 * Used for R-tree spatial indexing and query optimization.
 */
struct BoundingBox
{
    double min_x;  // Minimum X coordinate
    double min_y;  // Minimum Y coordinate
    double max_x;  // Maximum X coordinate
    double max_y;  // Maximum Y coordinate

    BoundingBox() : min_x(0.0), min_y(0.0), max_x(0.0), max_y(0.0) {}

    BoundingBox(double min_x, double min_y, double max_x, double max_y)
        : min_x(min_x), min_y(min_y), max_x(max_x), max_y(max_y)
    {
    }

    // Calculate area of the bounding box
    double area() const;

    // Calculate perimeter of the bounding box
    double perimeter() const;

    // Check if this box contains another box
    bool contains(const BoundingBox& other) const;

    // Check if this box intersects with another box
    bool intersects(const BoundingBox& other) const;

    // Calculate the enlargement needed to include another box
    double enlargement(const BoundingBox& other) const;

    // Calculate overlap area with another box
    double overlap(const BoundingBox& other) const;

    // Expand this box to include another box
    void expand(const BoundingBox& other);

    // Merge two boxes into a new box
    static BoundingBox merge(const BoundingBox& a, const BoundingBox& b);

    // Check if the box is valid (min <= max)
    bool isValid() const;

    // Get center point
    void getCenter(double* x, double* y) const;
};

/**
 * RTreeEntry - Entry in an R-tree node
 *
 * For leaf nodes: Contains bounding box and tuple ID (row_id)
 * For internal nodes: Contains bounding box and child node pointer
 */
struct RTreeEntry
{
    BoundingBox bbox;           // Minimum bounding rectangle

    // Union for leaf vs internal node entries
    union {
        TID row_id;             // For leaf nodes: points to heap tuple
        uint64_t child_page;    // For internal nodes: page number of child node
    };

    // MGA compliance
    uint64_t xmin;              // Transaction that created this entry
    uint64_t xmax;              // Transaction that deleted this entry (0 if active)

    bool is_deleted;            // True if logically deleted (xmax != 0)

    RTreeEntry() : xmin(0), xmax(0), is_deleted(false)
    {
        child_page = 0;
    }
};

/**
 * RTreeNode - Node in an R-tree
 *
 * Represents either a leaf node (contains data entries) or
 * an internal node (contains child node pointers).
 *
 * R-tree properties:
 * - All leaves are at the same level
 * - Internal nodes contain (child, MBR) pairs
 * - Leaf nodes contain (data, MBR) pairs
 * - Each node has between m and M entries (except root)
 */
class RTreeNode
{
public:
    /**
     * Constructor for a new R-tree node
     *
     * @param is_leaf True if this is a leaf node
     * @param max_entries Maximum number of entries per node
     */
    RTreeNode(bool is_leaf, uint32_t max_entries);

    /**
     * Destructor
     */
    ~RTreeNode();

    // Node type queries
    bool isLeaf() const { return is_leaf_; }
    bool isRoot() const { return parent_ == nullptr; }

    // Entry management
    size_t getEntryCount() const { return entries_.size(); }
    const RTreeEntry& getEntry(size_t index) const { return entries_[index]; }
    RTreeEntry& getEntry(size_t index) { return entries_[index]; }
    const std::vector<RTreeEntry>& getEntries() const { return entries_; }

    // Add an entry to this node
    void addEntry(const RTreeEntry& entry);

    // Remove an entry at the given index
    void removeEntry(size_t index);

    // Check if node is full
    bool isFull() const { return entries_.size() >= max_entries_; }

    // Check if node has minimum entries (for underflow detection)
    bool hasMinimumEntries() const;

    // Calculate the MBR of all entries in this node
    BoundingBox calculateMBR() const;

    // Find the entry with minimum enlargement to include bbox
    size_t findMinEnlargement(const BoundingBox& bbox) const;

    // Find the entry with minimum overlap increase
    size_t findMinOverlap(const BoundingBox& bbox) const;

    // Split this node into two nodes (R*-tree quadratic split)
    // Returns the new sibling node
    std::unique_ptr<RTreeNode> split();

    // Parent/child navigation
    RTreeNode* getParent() const { return parent_; }
    void setParent(RTreeNode* parent) { parent_ = parent; }

    // Page management (for persistence)
    uint64_t getPageNumber() const { return page_number_; }
    void setPageNumber(uint64_t page_num) { page_number_ = page_num; }

    // MGA transaction tracking
    uint64_t getXmin() const { return xmin_; }
    void setXmin(uint64_t xmin) { xmin_ = xmin; }
    uint64_t getXmax() const { return xmax_; }
    void setXmax(uint64_t xmax) { xmax_ = xmax; }

    // Level tracking (0 = leaf, increases upward)
    uint16_t getLevel() const { return level_; }
    void setLevel(uint16_t level) { level_ = level; }

private:
    bool is_leaf_;                          // True if leaf node, false if internal
    uint32_t max_entries_;                  // Maximum entries per node (M)
    std::vector<RTreeEntry> entries_;       // Entries in this node

    RTreeNode* parent_;                     // Parent node (nullptr for root)
    uint64_t page_number_;                  // Page number for persistence
    uint16_t level_;                        // Tree level (0 = leaf)

    // MGA compliance
    uint64_t xmin_;                         // Transaction that created this node
    uint64_t xmax_;                         // Transaction that deleted this node

    // Helper methods for split algorithm
    void quadraticSplit(std::vector<RTreeEntry>& group1,
                       std::vector<RTreeEntry>& group2);

    void pickSeeds(size_t* seed1, size_t* seed2) const;
    size_t pickNext(const BoundingBox& mbr1, const BoundingBox& mbr2,
                   const std::vector<bool>& assigned) const;
};

} // namespace scratchbird::core
