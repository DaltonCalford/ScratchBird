#include "scratchbird/engine/index_rtree.h"

#include "scratchbird/engine/ods.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace scratchbird::engine
{

    // RTreeLeaf Implementation
    bool RTreeLeaf::insert(const RTreeEntry& entry, RTreeNode*& split_node)
    {
        entries_.push_back(entry);

        if (entries_.size() > RTreeConfig::MAX_ENTRIES) {
            split_node = split();
            return true; // Split occurred
        }

        split_node = nullptr;
        return false; // No split
    }

    void RTreeLeaf::search(const Rectangle& query, std::vector<RTreeEntry>& results)
    {
        for (const auto& entry : entries_) {
            if (entry.rect.intersects(query)) {
                results.push_back(entry);
            }
        }
    }

    bool RTreeLeaf::remove(const Rectangle& rect, std::uint64_t row_id)
    {
        auto it = std::find_if(entries_.begin(), entries_.end(), [&](const RTreeEntry& entry) {
            return entry.row_id == row_id && std::abs(entry.rect.min_x - rect.min_x) < 1e-9 &&
                   std::abs(entry.rect.min_y - rect.min_y) < 1e-9 &&
                   std::abs(entry.rect.max_x - rect.max_x) < 1e-9 &&
                   std::abs(entry.rect.max_y - rect.max_y) < 1e-9;
        });

        if (it != entries_.end()) {
            entries_.erase(it);
            return true;
        }
        return false;
    }

    RTreeNode* RTreeLeaf::split()
    {
        // Quadratic split algorithm for leaf node
        auto new_leaf = std::make_unique<RTreeLeaf>();
        new_leaf->set_level(level_);

        // Find the pair of entries with maximum waste of area
        std::size_t seed1 = 0, seed2 = 1;
        double max_waste = -1.0;

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            for (std::size_t j = i + 1; j < entries_.size(); ++j) {
                Rectangle mbr = Rectangle::mbr(entries_[i].rect, entries_[j].rect);
                double waste = mbr.area() - entries_[i].rect.area() - entries_[j].rect.area();
                if (waste > max_waste) {
                    max_waste = waste;
                    seed1 = i;
                    seed2 = j;
                }
            }
        }

        // Initialize groups with seed entries
        std::vector<RTreeEntry> group1, group2;
        group1.push_back(entries_[seed1]);
        group2.push_back(entries_[seed2]);

        // Mark seeds as used
        std::vector<bool> used(entries_.size(), false);
        used[seed1] = used[seed2] = true;

        // Distribute remaining entries
        for (std::size_t remaining = entries_.size() - 2; remaining > 0; --remaining) {
            // If one group needs all remaining entries to meet minimum, assign them
            if (group1.size() + remaining == RTreeConfig::MIN_ENTRIES) {
                for (std::size_t i = 0; i < entries_.size(); ++i) {
                    if (!used[i]) {
                        group1.push_back(entries_[i]);
                        used[i] = true;
                    }
                }
                break;
            }
            if (group2.size() + remaining == RTreeConfig::MIN_ENTRIES) {
                for (std::size_t i = 0; i < entries_.size(); ++i) {
                    if (!used[i]) {
                        group2.push_back(entries_[i]);
                        used[i] = true;
                    }
                }
                break;
            }

            // Find next entry and decide which group to assign it to
            std::size_t next_entry = 0;
            for (std::size_t i = 0; i < entries_.size(); ++i) {
                if (!used[i]) {
                    next_entry = i;
                    break;
                }
            }

            // Calculate MBR for each group
            Rectangle mbr1 = group1[0].rect;
            Rectangle mbr2 = group2[0].rect;
            for (std::size_t i = 1; i < group1.size(); ++i)
                mbr1.expand(group1[i].rect);
            for (std::size_t i = 1; i < group2.size(); ++i)
                mbr2.expand(group2[i].rect);

            // Calculate expansion needed for each group
            double exp1 = mbr1.expansion_area(entries_[next_entry].rect);
            double exp2 = mbr2.expansion_area(entries_[next_entry].rect);

            // Assign to group with least expansion (tie-break by smaller area)
            if (exp1 < exp2 || (exp1 == exp2 && mbr1.area() < mbr2.area())) {
                group1.push_back(entries_[next_entry]);
            } else {
                group2.push_back(entries_[next_entry]);
            }
            used[next_entry] = true;
        }

        // Update this node with group1, new node gets group2
        entries_ = std::move(group1);
        new_leaf->entries_ = std::move(group2);

        return new_leaf.release();
    }

    // RTreeInternal Implementation
    bool RTreeInternal::insert(const RTreeEntry& entry, RTreeNode*& split_node)
    {
        entries_.push_back(entry);

        if (entries_.size() > RTreeConfig::MAX_ENTRIES) {
            split_node = split();
            return true; // Split occurred
        }

        split_node = nullptr;
        return false; // No split
    }

    void RTreeInternal::search(const Rectangle& query, std::vector<RTreeEntry>& results)
    {
        if (!children_)
            return;

        for (const auto& entry : entries_) {
            if (entry.rect.intersects(query)) {
                // Find the child node and recursively search
                if (entry.child_page < children_->size()) {
                    (*children_)[entry.child_page]->search(query, results);
                }
            }
        }
    }

    bool RTreeInternal::remove(const Rectangle& rect, std::uint64_t row_id)
    {
        if (!children_)
            return false;

        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->rect.intersects(rect)) {
                if (it->child_page < children_->size()) {
                    if ((*children_)[it->child_page]->remove(rect, row_id)) {
                        // Entry was removed from child, update MBR if needed
                        it->rect = (*children_)[it->child_page]->bounding_rect();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    std::uint32_t RTreeInternal::choose_subtree(const Rectangle& rect)
    {
        if (entries_.empty())
            return 0;

        std::uint32_t best_child = 0;
        double min_expansion = std::numeric_limits<double>::max();
        double min_area = std::numeric_limits<double>::max();

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            double expansion = entries_[i].rect.expansion_area(rect);
            double area = entries_[i].rect.area();

            if (expansion < min_expansion || (expansion == min_expansion && area < min_area)) {
                min_expansion = expansion;
                min_area = area;
                best_child = static_cast<std::uint32_t>(i);
            }
        }

        return best_child;
    }

    RTreeNode* RTreeInternal::split()
    {
        // Similar quadratic split for internal nodes
        auto new_internal = std::make_unique<RTreeInternal>();
        new_internal->set_level(level_);
        new_internal->set_children(*children_);

        // Use same quadratic split algorithm as leaf
        std::size_t seed1 = 0, seed2 = 1;
        double max_waste = -1.0;

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            for (std::size_t j = i + 1; j < entries_.size(); ++j) {
                Rectangle mbr = Rectangle::mbr(entries_[i].rect, entries_[j].rect);
                double waste = mbr.area() - entries_[i].rect.area() - entries_[j].rect.area();
                if (waste > max_waste) {
                    max_waste = waste;
                    seed1 = i;
                    seed2 = j;
                }
            }
        }

        std::vector<RTreeEntry> group1, group2;
        group1.push_back(entries_[seed1]);
        group2.push_back(entries_[seed2]);

        std::vector<bool> used(entries_.size(), false);
        used[seed1] = used[seed2] = true;

        for (std::size_t remaining = entries_.size() - 2; remaining > 0; --remaining) {
            if (group1.size() + remaining == RTreeConfig::MIN_ENTRIES) {
                for (std::size_t i = 0; i < entries_.size(); ++i) {
                    if (!used[i]) {
                        group1.push_back(entries_[i]);
                        used[i] = true;
                    }
                }
                break;
            }
            if (group2.size() + remaining == RTreeConfig::MIN_ENTRIES) {
                for (std::size_t i = 0; i < entries_.size(); ++i) {
                    if (!used[i]) {
                        group2.push_back(entries_[i]);
                        used[i] = true;
                    }
                }
                break;
            }

            std::size_t next_entry = 0;
            for (std::size_t i = 0; i < entries_.size(); ++i) {
                if (!used[i]) {
                    next_entry = i;
                    break;
                }
            }

            Rectangle mbr1 = group1[0].rect;
            Rectangle mbr2 = group2[0].rect;
            for (std::size_t i = 1; i < group1.size(); ++i)
                mbr1.expand(group1[i].rect);
            for (std::size_t i = 1; i < group2.size(); ++i)
                mbr2.expand(group2[i].rect);

            double exp1 = mbr1.expansion_area(entries_[next_entry].rect);
            double exp2 = mbr2.expansion_area(entries_[next_entry].rect);

            if (exp1 < exp2 || (exp1 == exp2 && mbr1.area() < mbr2.area())) {
                group1.push_back(entries_[next_entry]);
            } else {
                group2.push_back(entries_[next_entry]);
            }
            used[next_entry] = true;
        }

        entries_ = std::move(group1);
        new_internal->entries_ = std::move(group2);

        return new_internal.release();
    }

    // RTreeIndex Implementation
    RTreeIndex::RTreeIndex(FileMap fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size), unique_(unique)
    {
        if (unique_) {
            throw std::invalid_argument("R-Tree indexes cannot be unique");
        }
    }

    void RTreeIndex::create_empty()
    {
        try {
            root_ = std::make_unique<RTreeLeaf>();
            root_->set_level(0);
            height_ = 1;
            total_entries_ = 0;
            root_page_no_ = allocate_page(); // Allocate root page

            // Save the empty root node to disk
            if (!save_node(root_page_no_, root_.get())) {
                throw std::runtime_error("Failed to save root node during R-Tree creation");
            }
        } catch (const std::exception& e) {
            // Cleanup on failure
            root_.reset();
            node_pages_.clear();
            throw std::runtime_error("R-Tree create_empty failed: " + std::string(e.what()));
        }
    }

    std::uint32_t RTreeIndex::root_page() const
    {
        return root_page_no_;
    }

    bool RTreeIndex::open_existing(std::uint32_t root_page)
    {
        try {
            root_page_no_ = root_page;

            // Load the root node from disk
            if (!load_node(root_page_no_, root_)) {
                return false; // Failed to load root
            }

            // Calculate tree height and total entries
            height_ = calculate_tree_height(root_.get());
            total_entries_ = calculate_total_entries(root_.get());

            return true;
        } catch (const std::exception&) {
            root_.reset();
            return false;
        }
    }

    Rectangle RTreeIndex::parse_rectangle(const std::string& wkt_or_bbox) const
    {
        // Simple parser for WKT POLYGON or BBOX format
        // POLYGON((x1 y1, x2 y1, x2 y2, x1 y2, x1 y1))
        // BBOX(x1,y1,x2,y2)

        if (wkt_or_bbox.find("POLYGON") == 0) {
            // Extract coordinates from POLYGON WKT
            std::size_t start = wkt_or_bbox.find("((") + 2;
            std::size_t end = wkt_or_bbox.find("))");
            if (start == std::string::npos || end == std::string::npos) {
                throw std::invalid_argument("Invalid WKT polygon format");
            }

            std::string coords = wkt_or_bbox.substr(start, end - start);
            std::istringstream ss(coords);
            std::string coord;
            std::vector<std::pair<double, double>> points;

            while (std::getline(ss, coord, ',')) {
                std::istringstream coord_ss(coord);
                double x, y;
                if (coord_ss >> x >> y) {
                    points.emplace_back(x, y);
                }
            }

            if (points.size() < 4) {
                throw std::invalid_argument("Polygon must have at least 4 points");
            }

            // Calculate bounding rectangle
            double min_x = points[0].first, max_x = points[0].first;
            double min_y = points[0].second, max_y = points[0].second;

            for (const auto& point : points) {
                min_x = std::min(min_x, point.first);
                max_x = std::max(max_x, point.first);
                min_y = std::min(min_y, point.second);
                max_y = std::max(max_y, point.second);
            }

            return Rectangle(min_x, min_y, max_x, max_y);
        } else if (wkt_or_bbox.find("BBOX") == 0) {
            // Parse BBOX(x1,y1,x2,y2) format
            std::size_t start = wkt_or_bbox.find('(') + 1;
            std::size_t end = wkt_or_bbox.find(')');
            if (start == std::string::npos || end == std::string::npos) {
                throw std::invalid_argument("Invalid BBOX format");
            }

            std::string coords = wkt_or_bbox.substr(start, end - start);
            std::istringstream ss(coords);
            std::string coord;
            std::vector<double> values;

            while (std::getline(ss, coord, ',')) {
                values.push_back(std::stod(coord));
            }

            if (values.size() != 4) {
                throw std::invalid_argument("BBOX must have exactly 4 coordinates");
            }

            return Rectangle(values[0], values[1], values[2], values[3]);
        } else {
            // Try to parse as comma-separated coordinates: "x1,y1,x2,y2"
            std::istringstream ss(wkt_or_bbox);
            std::string coord;
            std::vector<double> values;

            while (std::getline(ss, coord, ',')) {
                values.push_back(std::stod(coord));
            }

            if (values.size() == 4) {
                return Rectangle(values[0], values[1], values[2], values[3]);
            }

            throw std::invalid_argument("Unsupported spatial format");
        }
    }

    std::string RTreeIndex::format_rectangle(const Rectangle& rect) const
    {
        std::ostringstream oss;
        oss << "BBOX(" << rect.min_x << "," << rect.min_y << "," << rect.max_x << "," << rect.max_y
            << ")";
        return oss.str();
    }

    bool RTreeIndex::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        try {
            Rectangle rect = parse_rectangle(key);
            RTreeEntry entry(rect, row_id);

            std::vector<RTreeNode*> path;
            insert_recursive(root_.get(), entry, path, height_ - 1);
            total_entries_++;
            insert_count_++;

            return true;
        } catch (const std::exception& e) {
            err = "R-Tree insert error: " + std::string(e.what());
            return false;
        }
    }

    bool RTreeIndex::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                         const std::string& payload, std::string& err)
    {
        try {
            Rectangle rect = parse_rectangle(key);
            RTreeEntry entry(rect, row_id);
            entry.payload = payload;

            std::vector<RTreeNode*> path;
            insert_recursive(root_.get(), entry, path, height_ - 1);
            total_entries_++;
            insert_count_++;

            return true;
        } catch (const std::exception& e) {
            err = "R-Tree insert with payload error: " + std::string(e.what());
            return false;
        }
    }

    void RTreeIndex::search_equal(const std::string& key, std::vector<std::uint64_t>& out) const
    {
        try {
            Rectangle rect = parse_rectangle(key);
            std::vector<RTreeEntry> entries;
            if (root_) {
                root_->search(rect, entries);
            }

            for (const auto& entry : entries) {
                // For "equal" search, we look for exact rectangle matches
                if (std::abs(entry.rect.min_x - rect.min_x) < 1e-9 &&
                    std::abs(entry.rect.min_y - rect.min_y) < 1e-9 &&
                    std::abs(entry.rect.max_x - rect.max_x) < 1e-9 &&
                    std::abs(entry.rect.max_y - rect.max_y) < 1e-9) {
                    out.push_back(entry.row_id);
                }
            }
            search_count_++;
        } catch (const std::exception&) {
            // Invalid spatial format, return no results
        }
    }

    void RTreeIndex::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        try {
            Rectangle rect = parse_rectangle(key);
            std::vector<RTreeEntry> entries;
            if (root_) {
                root_->search(rect, entries);
            }

            for (const auto& entry : entries) {
                if (std::abs(entry.rect.min_x - rect.min_x) < 1e-9 &&
                    std::abs(entry.rect.min_y - rect.min_y) < 1e-9 &&
                    std::abs(entry.rect.max_x - rect.max_x) < 1e-9 &&
                    std::abs(entry.rect.max_y - rect.max_y) < 1e-9) {
                    out.emplace_back(entry.row_id, entry.payload);
                }
            }
            search_count_++;
        } catch (const std::exception&) {
            // Invalid spatial format, return no results
        }
    }

    void RTreeIndex::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                  bool hi_incl,
                                  std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        // For R-Tree, range query is interpreted as intersection query
        try {
            Rectangle query_rect = parse_rectangle(lo);
            std::vector<RTreeEntry> entries;
            if (root_) {
                root_->search(query_rect, entries);
            }

            for (const auto& entry : entries) {
                out.emplace_back(format_rectangle(entry.rect), entry.row_id);
            }
            search_count_++;
        } catch (const std::exception&) {
            // Invalid spatial format, return no results
        }
    }

    void RTreeIndex::search_intersects(const Rectangle& query, std::vector<RTreeEntry>& results)
    {
        root_->search(query, results);
        search_count_++;
    }

    void RTreeIndex::search_contains(const Rectangle& query, std::vector<RTreeEntry>& results)
    {
        std::vector<RTreeEntry> intersecting;
        root_->search(query, intersecting);

        for (const auto& entry : intersecting) {
            if (entry.rect.contains(query)) {
                results.push_back(entry);
            }
        }
        search_count_++;
    }

    void RTreeIndex::search_within(const Rectangle& query, std::vector<RTreeEntry>& results)
    {
        std::vector<RTreeEntry> intersecting;
        root_->search(query, intersecting);

        for (const auto& entry : intersecting) {
            if (query.contains(entry.rect)) {
                results.push_back(entry);
            }
        }
        search_count_++;
    }

    std::size_t RTreeIndex::erase_equal(const std::string& key, std::string& err)
    {
        try {
            Rectangle rect = parse_rectangle(key);
            std::size_t removed = 0;

            // Find all matching entries
            std::vector<RTreeEntry> entries;
            if (root_) {
                root_->search(rect, entries);
            }

            for (const auto& entry : entries) {
                if (std::abs(entry.rect.min_x - rect.min_x) < 1e-9 &&
                    std::abs(entry.rect.min_y - rect.min_y) < 1e-9 &&
                    std::abs(entry.rect.max_x - rect.max_x) < 1e-9 &&
                    std::abs(entry.rect.max_y - rect.max_y) < 1e-9) {
                    if (root_->remove(rect, entry.row_id)) {
                        removed++;
                        total_entries_--;
                    }
                }
            }

            return removed;
        } catch (const std::exception& e) {
            err = "R-Tree erase error: " + std::string(e.what());
            return 0;
        }
    }

    void RTreeIndex::insert_recursive(RTreeNode* node, const RTreeEntry& entry,
                                      std::vector<RTreeNode*>& path, std::uint32_t level)
    {
        path.push_back(node);

        if (node->is_leaf()) {
            RTreeNode* split_node = nullptr;
            if (node->insert(entry, split_node)) {
                handle_overflow(node, path, level);
            }
        } else {
            // Choose subtree for insertion
            auto* internal = static_cast<RTreeInternal*>(node);
            std::uint32_t child_idx = internal->choose_subtree(entry.rect);

            if (child_idx < nodes_.size()) {
                insert_recursive(nodes_[child_idx].get(), entry, path, level - 1);
            }
        }
    }

    void RTreeIndex::handle_overflow(RTreeNode* node, const std::vector<RTreeNode*>& path,
                                     std::uint32_t level)
    {
        // Simplified overflow handling - just split
        split_count_++;

        if (path.size() == 1) {
            // Root overflow - create new root
            split_root();
        }

        adjust_tree(path);
    }

    void RTreeIndex::split_root()
    {
        auto new_root = std::make_unique<RTreeInternal>();
        new_root->set_level(height_);

        // Add the old root as first child
        Rectangle root_mbr = root_->bounding_rect();
        std::uint32_t old_root_page = static_cast<std::uint32_t>(nodes_.size());
        nodes_.push_back(std::move(root_));

        RTreeEntry root_entry(root_mbr, old_root_page);
        RTreeNode* split_node = nullptr;
        new_root->insert(root_entry, split_node);

        root_ = std::move(new_root);
        height_++;
    }

    void RTreeIndex::adjust_tree(const std::vector<RTreeNode*>& path)
    {
        // Simplified tree adjustment - update MBRs up the path
        for (std::size_t i = path.size(); i > 0; --i) {
            RTreeNode* node = path[i - 1];
            if (!node->is_leaf() && i > 1) {
                // Update MBR for parent entry
                auto* internal = static_cast<RTreeInternal*>(node);
                for (auto& entry : internal->entries()) {
                    if (entry.child_page < nodes_.size()) {
                        entry.rect = nodes_[entry.child_page]->bounding_rect();
                    }
                }
            }
        }
    }

    void RTreeIndex::rebuild_offline()
    {
        // TODO: Implement R-Tree rebuild - recreate tree from all entries
        // For now, just clear statistics
        search_count_ = 0;
        insert_count_ = 0;
        split_count_ = 0;
        pages_accessed_ = 0;
    }

    void RTreeIndex::compact_index()
    {
        // TODO: Implement R-Tree compaction - remove dead space, reorganize pages
        // For now, this is a no-op
    }

    bool RTreeIndex::validate(std::string& error) const
    {
        if (!root_) {
            error = "No root node";
            return false;
        }

        // Basic validation - could be expanded
        error.clear();
        return true;
    }

    std::string RTreeIndex::collect_statistics() const
    {
        std::ostringstream stats;
        stats << "R-Tree Index Statistics:\n"
              << "  Total entries: " << total_entries_ << "\n"
              << "  Tree height: " << height_ << "\n"
              << "  Search operations: " << search_count_ << "\n"
              << "  Insert operations: " << insert_count_ << "\n"
              << "  Split operations: " << split_count_ << "\n"
              << "  Pages accessed: " << pages_accessed_ << "\n"
              << "  Root MBR: " << (root_ ? format_rectangle(root_->bounding_rect()) : "empty")
              << "\n";
        return stats.str();
    }

    double RTreeIndex::estimate_search_cost(const std::string& key) const
    {
        // R-Tree search cost is roughly O(log n) for point queries
        // Spatial queries might need to access multiple paths
        return std::log2(static_cast<double>(total_entries_ + 1)) * 1.2;
    }

    double RTreeIndex::estimate_range_cost(const std::string& lo, const std::string& hi) const
    {
        // Range queries (spatial intersection) can be more expensive
        // Depends on query rectangle size and data distribution
        double base_cost = std::log2(static_cast<double>(total_entries_ + 1));
        return base_cost * 2.0; // Spatial range queries typically more expensive
    }

    double RTreeIndex::estimate_maintenance_cost() const
    {
        // R-Tree maintenance includes splits and MBR updates
        // Cost increases with tree height and number of entries
        return static_cast<double>(height_) * std::log2(static_cast<double>(total_entries_ + 1)) *
               0.1;
    }

    std::unique_ptr<RTreeNode> RTreeIndex::create_node(bool is_leaf)
    {
        if (is_leaf) {
            return std::make_unique<RTreeLeaf>();
        } else {
            auto internal = std::make_unique<RTreeInternal>();
            internal->set_children(nodes_);
            return internal;
        }
    }

    std::uint32_t RTreeIndex::allocate_page()
    {
        pages_accessed_++;
        return static_cast<std::uint32_t>(nodes_.size());
    }

    bool RTreeIndex::load_node(std::uint32_t page_no, std::unique_ptr<RTreeNode>& node) const
    {
        try {
            pages_accessed_++;

            // Read page from FileMap
            std::vector<std::uint8_t> page_data(page_size_);
            // For now, use in-memory storage to avoid FileMap complexity
            auto it = node_pages_.find(page_no);
            if (it == node_pages_.end()) {
                return false; // Page doesn't exist
            }

            page_data = it->second;

            // Parse page header
            if (page_data.size() < sizeof(RTreeNodeHeader)) {
                return false;
            }

            RTreeNodeHeader header;
            std::memcpy(&header, page_data.data(), sizeof(header));

            // Create appropriate node type
            if (header.is_leaf) {
                node = std::make_unique<RTreeLeaf>();
            } else {
                node = std::make_unique<RTreeInternal>();
            }

            node->set_level(header.level);

            // Deserialize entries
            std::size_t offset = sizeof(RTreeNodeHeader);
            for (std::uint32_t i = 0; i < header.entry_count; ++i) {
                if (offset + sizeof(RTreeEntry) > page_data.size()) {
                    return false; // Corrupted page
                }

                RTreeEntry entry;
                std::memcpy(&entry.rect, page_data.data() + offset, sizeof(Rectangle));
                offset += sizeof(Rectangle);

                std::memcpy(&entry.child_page, page_data.data() + offset, sizeof(std::uint32_t));
                offset += sizeof(std::uint32_t);

                std::memcpy(&entry.row_id, page_data.data() + offset, sizeof(std::uint64_t));
                offset += sizeof(std::uint64_t);

                // Read payload length and data
                std::uint32_t payload_len;
                std::memcpy(&payload_len, page_data.data() + offset, sizeof(std::uint32_t));
                offset += sizeof(std::uint32_t);

                if (payload_len > 0) {
                    if (offset + payload_len > page_data.size()) {
                        return false; // Corrupted payload
                    }
                    entry.payload.assign(reinterpret_cast<const char*>(page_data.data() + offset),
                                         payload_len);
                    offset += payload_len;
                }

                // Add entry to node through accessor
                node->get_entries().push_back(entry);
            }

            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    bool RTreeIndex::save_node(std::uint32_t page_no, const RTreeNode* node)
    {
        try {
            pages_accessed_++;

            std::vector<std::uint8_t> page_data;
            page_data.reserve(page_size_);

            // Create page header
            RTreeNodeHeader header;
            header.node_type = static_cast<std::uint32_t>(ods::PageType::RTreeNode);
            header.is_leaf = node->is_leaf();
            header.level = node->level();
            header.parent_page = 0; // Would be set by caller in real implementation

            // Get entries from node through accessor
            const std::vector<RTreeEntry>& entries = node->get_entries();

            header.entry_count = entries.size();

            // Write header
            page_data.resize(sizeof(RTreeNodeHeader));
            std::memcpy(page_data.data(), &header, sizeof(header));

            // Serialize entries
            for (const auto& entry : entries) {
                // Write rectangle
                std::size_t old_size = page_data.size();
                page_data.resize(old_size + sizeof(Rectangle));
                std::memcpy(page_data.data() + old_size, &entry.rect, sizeof(Rectangle));

                // Write child_page
                old_size = page_data.size();
                page_data.resize(old_size + sizeof(std::uint32_t));
                std::memcpy(page_data.data() + old_size, &entry.child_page, sizeof(std::uint32_t));

                // Write row_id
                old_size = page_data.size();
                page_data.resize(old_size + sizeof(std::uint64_t));
                std::memcpy(page_data.data() + old_size, &entry.row_id, sizeof(std::uint64_t));

                // Write payload
                std::uint32_t payload_len = entry.payload.size();
                old_size = page_data.size();
                page_data.resize(old_size + sizeof(std::uint32_t));
                std::memcpy(page_data.data() + old_size, &payload_len, sizeof(std::uint32_t));

                if (payload_len > 0) {
                    old_size = page_data.size();
                    page_data.resize(old_size + payload_len);
                    std::memcpy(page_data.data() + old_size, entry.payload.data(), payload_len);
                }
            }

            // Pad to page size
            if (page_data.size() < page_size_) {
                page_data.resize(page_size_, 0);
            }

            // Store in memory (simulate disk storage)
            node_pages_[page_no] = page_data;

            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    // Helper methods for tree statistics
    std::uint32_t RTreeIndex::calculate_tree_height(const RTreeNode* node) const
    {
        if (!node || node->is_leaf()) {
            return 1;
        }

        // For internal nodes, recursively calculate height
        std::uint32_t max_child_height = 0;
        const auto& entries = node->get_entries();

        for (const auto& entry : entries) {
            std::unique_ptr<RTreeNode> child_node;
            if (load_node(entry.child_page, child_node)) {
                std::uint32_t child_height = calculate_tree_height(child_node.get());
                max_child_height = std::max(max_child_height, child_height);
            }
        }

        return 1 + max_child_height;
    }

    std::uint64_t RTreeIndex::calculate_total_entries(const RTreeNode* node) const
    {
        if (!node) {
            return 0;
        }

        if (node->is_leaf()) {
            return node->get_entries().size();
        }

        // For internal nodes, recursively count entries
        std::uint64_t total = 0;
        const auto& entries = node->get_entries();

        for (const auto& entry : entries) {
            std::unique_ptr<RTreeNode> child_node;
            if (load_node(entry.child_page, child_node)) {
                total += calculate_total_entries(child_node.get());
            }
        }

        return total;
    }

} // namespace scratchbird::engine
