// Minimal tests for structured join tree
#include "scratchbird/engine/parser_select.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // Nested join-group becomes a non-leaf with children
    {
        auto q =
            parse_select_minimal("SELECT * FROM t JOIN (u JOIN v ON u.id=v.uid) ON t.id=u.tid");
        assert(q.join_tree_root);
        assert(!q.join_tree_root->is_leaf);
        assert(q.join_tree_root->left && q.join_tree_root->right);
        // Right subtree should itself be a non-leaf due to (u JOIN v ...)
        assert(!q.join_tree_root->right->is_leaf);
        // ON captured on root
        assert(q.join_tree_root->has_on);
    }
    // NATURAL JOIN flag propagates
    {
        auto q = parse_select_minimal("SELECT * FROM t NATURAL JOIN u");
        assert(q.join_tree_root);
        assert(q.join_tree_root->natural);
        assert(!q.join_tree_root->has_on);
        // USING is not set for NATURAL in our model
        assert(!q.join_tree_root->has_using);
    }
    // USING list on join
    {
        auto q = parse_select_minimal("SELECT * FROM t JOIN u USING(id, (x))");
        assert(q.join_tree_root);
        assert(q.join_tree_root->has_using);
        assert(!q.join_tree_root->using_cols.empty());
    }
    // Table function on RHS forms a leaf subquery
    {
        auto q = parse_select_minimal("SELECT * FROM t JOIN my_func(1, 2) f ON t.id=f.id");
        assert(q.join_tree_root);
        assert(q.join_tree_root->right);
        assert(q.join_tree_root->right->is_leaf);
        assert(q.join_tree_root->right->leaf.is_subquery);
        assert(q.join_tree_root->right->leaf.alias == "f");
    }
    return 0;
}
