#ifndef SCRATCHBIRD_ENGINE_INDEX_RTREE_H
#define SCRATCHBIRD_ENGINE_INDEX_RTREE_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Forward declarations
    class RTreeNode;
    class RTreeLeaf;
    class RTreeInternal;

    /// @brief Rectangle structure for spatial operations
    struct Rectangle {
        double min_x{0.0}, min_y{0.0}, max_x{0.0}, max_y{0.0};

        Rectangle() = default;
        Rectangle(double mx, double my, double mx2, double my2)
            : min_x(mx), min_y(my), max_x(mx2), max_y(my2)
        {
        }

        /// @brief Calculate area of rectangle
        double area() const
        {
            return (max_x - min_x) * (max_y - min_y);
        }

        /// @brief Calculate perimeter of rectangle
        double perimeter() const
        {
            return 2.0 * ((max_x - min_x) + (max_y - min_y));
        }

        /// @brief Check if this rectangle intersects with another
        bool intersects(const Rectangle& other) const
        {
            return !(max_x < other.min_x || min_x > other.max_x || max_y < other.min_y ||
                     min_y > other.max_y);
        }

        /// @brief Check if this rectangle contains another
        bool contains(const Rectangle& other) const
        {
            return min_x <= other.min_x && max_x >= other.max_x && min_y <= other.min_y &&
                   max_y >= other.max_y;
        }

        /// @brief Check if point is inside rectangle
        bool contains_point(double x, double y) const
        {
            return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
        }

        /// @brief Expand rectangle to include another rectangle
        Rectangle& expand(const Rectangle& other)
        {
            min_x = std::min(min_x, other.min_x);
            min_y = std::min(min_y, other.min_y);
            max_x = std::max(max_x, other.max_x);
            max_y = std::max(max_y, other.max_y);
            return *this;
        }

        /// @brief Calculate increase in area if expanding to include other rectangle
        double expansion_area(const Rectangle& other) const
        {
            Rectangle expanded = *this;
            expanded.expand(other);
            return expanded.area() - area();
        }

        /// @brief Calculate minimum bounding rectangle of two rectangles
        static Rectangle mbr(const Rectangle& a, const Rectangle& b)
        {
            return Rectangle(std::min(a.min_x, b.min_x), std::min(a.min_y, b.min_y),
                             std::max(a.max_x, b.max_x), std::max(a.max_y, b.max_y));
        }
    };

    /// @brief Entry in R-Tree (rectangle + pointer/row_id)
    struct RTreeEntry {
        Rectangle rect;
        std::uint32_t child_page{0}; // For internal nodes
        std::uint64_t row_id{0};     // For leaf nodes
        std::string payload;         // For INCLUDE columns

        RTreeEntry() = default;
        RTreeEntry(const Rectangle& r, std::uint64_t rid) : rect(r), row_id(rid) {}
        RTreeEntry(const Rectangle& r, std::uint32_t page) : rect(r), child_page(page) {}
    };

    /// @brief R-Tree node page header for disk persistence
    struct RTreeNodeHeader {
        std::uint32_t node_type;   // Page type (RTreeNode)
        std::uint32_t entry_count; // Number of entries in this node
        std::uint32_t parent_page; // Parent page number (0 for root)
        std::uint32_t level;       // Level in the tree (0 for leaves)
        bool is_leaf;              // True for leaf nodes
        std::uint8_t padding[3];   // Alignment padding
        Rectangle mbr;             // Minimum bounding rectangle for this node
    };

    /// @brief R-Tree configuration parameters
    struct RTreeConfig {
        static constexpr std::uint32_t MIN_ENTRIES = 2; // Minimum entries per node
        static constexpr std::uint32_t MAX_ENTRIES = 8; // Maximum entries per node
        static constexpr double REINSERT_FACTOR = 0.3;  // Fraction to reinsert on overflow
    };

    /// @brief Abstract base class for R-Tree nodes
    class RTreeNode
    {
      public:
        RTreeNode(bool is_leaf = false) : is_leaf_(is_leaf) {}
        virtual ~RTreeNode() = default;

        bool is_leaf() const
        {
            return is_leaf_;
        }
        std::uint32_t level() const
        {
            return level_;
        }
        void set_level(std::uint32_t level)
        {
            level_ = level;
        }

        const std::vector<RTreeEntry>& entries() const
        {
            return entries_;
        }
        std::vector<RTreeEntry>& entries()
        {
            return entries_;
        }

        Rectangle bounding_rect() const
        {
            if (entries_.empty())
                return Rectangle{};
            Rectangle mbr = entries_[0].rect;
            for (std::size_t i = 1; i < entries_.size(); ++i) {
                mbr.expand(entries_[i].rect);
            }
            return mbr;
        }

        virtual bool insert(const RTreeEntry& entry, RTreeNode*& split_node) = 0;
        virtual void search(const Rectangle& query, std::vector<RTreeEntry>& results) = 0;
        virtual bool remove(const Rectangle& rect, std::uint64_t row_id) = 0;

        // Accessor methods for persistence
        const std::vector<RTreeEntry>& get_entries() const
        {
            return entries_;
        }
        std::vector<RTreeEntry>& get_entries()
        {
            return entries_;
        }

      protected:
        bool is_leaf_;
        std::uint32_t level_{0};
        std::vector<RTreeEntry> entries_;
    };

    /// @brief R-Tree leaf node implementation
    class RTreeLeaf : public RTreeNode
    {
      public:
        RTreeLeaf() : RTreeNode(true) {}

        bool insert(const RTreeEntry& entry, RTreeNode*& split_node) override;
        void search(const Rectangle& query, std::vector<RTreeEntry>& results) override;
        bool remove(const Rectangle& rect, std::uint64_t row_id) override;

      private:
        RTreeNode* split();
    };

    /// @brief R-Tree internal node implementation
    class RTreeInternal : public RTreeNode
    {
      public:
        RTreeInternal() : RTreeNode(false) {}

        bool insert(const RTreeEntry& entry, RTreeNode*& split_node) override;
        void search(const Rectangle& query, std::vector<RTreeEntry>& results) override;
        bool remove(const Rectangle& rect, std::uint64_t row_id) override;

        void set_children(const std::vector<std::unique_ptr<RTreeNode>>& children)
        {
            children_ = &children;
        }

        std::uint32_t choose_subtree(const Rectangle& rect);

      private:
        RTreeNode* split();

        // Reference to children managed by RTreeIndex
        const std::vector<std::unique_ptr<RTreeNode>>* children_{nullptr};
    };

    /// @brief Complete R-Tree spatial index implementation
    class RTreeIndex : public IndexFamily
    {
      public:
        RTreeIndex(FileMap fmap, std::uint32_t page_size, bool unique = false);
        ~RTreeIndex() override = default;

        // IndexFamily interface
        IndexMethod get_method() const override
        {
            return IndexMethod::RTree;
        }
        void create_empty() override;
        std::uint32_t root_page() const override;
        bool open_existing(std::uint32_t root_page) override;

        bool insert(const std::string& key, std::uint64_t row_id, std::string& err) override;
        bool insert_with_payload(const std::string& key, std::uint64_t row_id,
                                 const std::string& payload, std::string& err) override;

        void search_equal(const std::string& key, std::vector<std::uint64_t>& out) const override;
        void search_equal_with_payload(
            const std::string& key,
            std::vector<std::pair<std::uint64_t, std::string>>& out) const override;

        void search_range(const std::string& lo, bool lo_incl, const std::string& hi, bool hi_incl,
                          std::vector<std::pair<std::string, std::uint64_t>>& out) const override;

        std::size_t erase_equal(const std::string& key, std::string& err) override;
        bool validate(std::string& error) const override;
        void rebuild_offline() override;
        std::string collect_statistics() const override;
        void compact_index() override;

        // Cost estimation methods
        double estimate_search_cost(const std::string& key) const override;
        double estimate_range_cost(const std::string& lo, const std::string& hi) const override;
        double estimate_maintenance_cost() const override;

        // Spatial-specific methods
        void search_intersects(const Rectangle& query, std::vector<RTreeEntry>& results);
        void search_contains(const Rectangle& query, std::vector<RTreeEntry>& results);
        void search_within(const Rectangle& query, std::vector<RTreeEntry>& results);

        // Utility methods
        Rectangle parse_rectangle(const std::string& wkt_or_bbox) const;
        std::string format_rectangle(const Rectangle& rect) const;

        // Access for scan operations
        RTreeNode* get_root() const
        {
            return root_.get();
        }
        const std::vector<std::unique_ptr<RTreeNode>>& get_nodes() const
        {
            return nodes_;
        }

      private:
        FileMap fmap_;
        std::uint32_t page_size_;
        bool unique_;
        std::uint32_t root_page_no_{0};

        std::unique_ptr<RTreeNode> root_;
        std::vector<std::unique_ptr<RTreeNode>> nodes_;
        std::uint32_t height_{1};
        std::uint64_t total_entries_{0};

        // Statistics
        mutable std::uint64_t search_count_{0};
        mutable std::uint64_t insert_count_{0};
        mutable std::uint64_t split_count_{0};
        mutable std::uint64_t pages_accessed_{0};

        // R-Tree algorithms
        void insert_recursive(RTreeNode* node, const RTreeEntry& entry,
                              std::vector<RTreeNode*>& path, std::uint32_t level);
        void handle_overflow(RTreeNode* node, const std::vector<RTreeNode*>& path,
                             std::uint32_t level);
        void split_root();
        void adjust_tree(const std::vector<RTreeNode*>& path);

        // Quadratic split algorithm
        std::pair<std::vector<RTreeEntry>, std::vector<RTreeEntry>>
        quadratic_split(const std::vector<RTreeEntry>& entries);

        // R* tree reinsertion
        void reinsert(RTreeNode* node, const std::vector<RTreeNode*>& path, std::uint32_t level);

        // Node management
        std::unique_ptr<RTreeNode> create_node(bool is_leaf);
        std::uint32_t allocate_page();
        bool load_node(std::uint32_t page_no, std::unique_ptr<RTreeNode>& node) const;
        bool save_node(std::uint32_t page_no, const RTreeNode* node);

        // Tree statistics calculation
        std::uint32_t calculate_tree_height(const RTreeNode* node) const;
        std::uint64_t calculate_total_entries(const RTreeNode* node) const;

        // In-memory page storage to simulate disk persistence
        mutable std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> node_pages_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_RTREE_H
