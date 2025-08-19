#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser_select.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{

    // Simple global plan cache accessor for optimizer textual plans
    struct LruCache {
        std::size_t capacity{128};
        std::list<std::pair<std::string, std::string>> items;
        std::unordered_map<std::string, std::list<std::pair<std::string, std::string>>::iterator>
            index;
        // new: relation/stat tracking
        std::unordered_map<std::string, std::vector<std::string>> key_to_relations;
        std::unordered_map<std::string, std::uint64_t> key_to_stats_epoch;
        void set(const std::string& k, const std::string& v)
        {
            auto it = index.find(k);
            if (it != index.end()) {
                it->second->second = v;
                items.splice(items.begin(), items, it->second);
                return;
            }
            items.emplace_front(k, v);
            index[k] = items.begin();
            if (index.size() > capacity) {
                auto last = items.end();
                --last;
                key_to_relations.erase(last->first);
                key_to_stats_epoch.erase(last->first);
                index.erase(last->first);
                items.pop_back();
            }
        }
        const std::string* get(const std::string& k)
        {
            auto it = index.find(k);
            if (it == index.end())
                return nullptr;
            items.splice(items.begin(), items, it->second);
            return &it->second->second;
        }
        void clear()
        {
            items.clear();
            index.clear();
            key_to_relations.clear();
            key_to_stats_epoch.clear();
        }
        void tag_relations(const std::string& k, const std::vector<std::string>& rels,
                           std::uint64_t epoch)
        {
            key_to_relations[k] = rels;
            key_to_stats_epoch[k] = epoch;
        }
        void invalidate_for_relation(const std::string& rel)
        {
            std::vector<std::string> to_erase;
            for (auto& kv : key_to_relations) {
                for (auto& r : kv.second)
                    if (r == rel) {
                        to_erase.push_back(kv.first);
                        break;
                    }
            }
            for (auto& k : to_erase) {
                auto it = index.find(k);
                if (it != index.end())
                    items.erase(it->second);
                index.erase(k);
                key_to_relations.erase(k);
                key_to_stats_epoch.erase(k);
            }
        }
    };

    static LruCache& get_plan_cache()
    {
        static LruCache cache;
        return cache;
    }

    // Planner result for a single SELECT
    struct PlanChoice {
        std::string method;     // result | seq_scan | idx_scan | idx_only
        std::string relation;   // base table when available
        std::string index_name; // chosen index if any
        double cost{0.0};
        double selectivity{1.0};
        std::uint64_t est_rows{0};
        bool order_satisfied{false};
        bool index_only{false};
    };

    static double cost_index_scan(double selectivity, double ndistinct, std::uint64_t total_rows)
    {
        // crude model: cost proportional to pages touched ~ selectivity * logN
        (void)ndistinct;
        double n = static_cast<double>(total_rows);
        double logn = (n > 1.0) ? std::log2(n) : 1.0;
        return std::max(1.0, selectivity * logn);
    }

    static double cost_seq_scan(std::uint64_t total_rows)
    {
        double n = static_cast<double>(total_rows);
        return std::max(1.0, std::sqrt(std::max(1.0, n))); // toy model
    }

    static double apply_fdw_costing(const std::string& rel, double base_cost, std::uint64_t rows)
    {
        if (const FdwCost* c = fdw_lookup_cost(rel)) {
            if (!c->is_fdw)
                return base_cost;
            double ms = c->startup_ms + c->per_row_ms * static_cast<double>(rows);
            // convert ms to cost units roughly 1ms->1cost for toy
            return base_cost + ms;
        }
        return base_cost;
    }

    static double estimate_selectivity_from_where(const std::string& where, double ndistinct)
    {
        if (where.empty())
            return 1.0;
        std::string lw = where;
        std::transform(lw.begin(), lw.end(), lw.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (lw.find(" = ") != std::string::npos) {
            double nd = ndistinct > 1.0 ? ndistinct : 1000.0;
            return std::min(1.0, 1.0 / nd);
        }
        if (lw.find(" in (") != std::string::npos) {
            int commas = 0;
            bool in_list = false;
            for (size_t i = 0; i < lw.size(); ++i) {
                if (!in_list && lw.compare(i, 4, " in ") == 0)
                    in_list = true;
                if (in_list && lw[i] == ',')
                    ++commas;
                if (in_list && lw[i] == ')')
                    break;
            }
            int k = commas + 1;
            double nd = ndistinct > 1.0 ? ndistinct : 1000.0;
            return std::min(1.0, std::max(1.0 / nd, double(k) / nd));
        }
        if (lw.find(" between ") != std::string::npos)
            return 0.1;
        if (lw.find(" like ") != std::string::npos)
            return 0.2;
        if (lw.find(">") != std::string::npos || lw.find("<") != std::string::npos)
            return 0.25;
        return 0.5;
    }

    // Naive sargability check: does WHERE reference this relation with typical predicates?
    static bool is_sargable_for_relation(const SelectQuery& q, const std::string& relation)
    {
        if (q.where_expr.empty())
            return false;
        std::string lw = q.where_expr;
        std::transform(lw.begin(), lw.end(), lw.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        std::string reldot = relation;
        std::transform(reldot.begin(), reldot.end(), reldot.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        reldot += ".";
        if (lw.find(reldot) == std::string::npos)
            return false;
        return lw.find(" = ") != std::string::npos || lw.find(" in (") != std::string::npos ||
               lw.find(" between ") != std::string::npos ||
               lw.find(" like ") != std::string::npos || lw.find('>') != std::string::npos ||
               lw.find('<') != std::string::npos;
    }

    static std::uint64_t estimate_rows_for_relation(const std::string& relation,
                                                    const std::string& where)
    {
        IndexStats st{};
        auto& reg = stats_registry();
        auto it = reg.find(relation);
        if (it != reg.end())
            st = it->second;
        else {
            st.key_count = 1000000;
            st.ndistinct = 100000;
        }
        double sel = estimate_selectivity_from_where(where, st.ndistinct);
        std::uint64_t rows = static_cast<std::uint64_t>(
            std::max(1.0, sel * (st.key_count > 0 ? double(st.key_count) : 100000.0)));
        return rows;
    }

    // (moved up)

    static bool order_by_satisfied(const std::vector<OrderItem>& order_by)
    {
        if (order_by.empty())
            return true;
        // Placeholder: assume a single-column ORDER BY can be satisfied by an index
        return order_by.size() == 1;
    }

    static bool is_index_only_candidate(const SelectQuery& q)
    {
        // Index-only when projecting literal or empty projection
        if (q.projections.empty())
            return true;
        for (auto const& p : q.projections) {
            // treat COUNT(*) and ordinals as index-only friendly
            if (p == "COUNT(*)" || p == "1" || p == "*")
                continue;
            // otherwise assume needs table fetch
            return false;
        }
        return true;
    }

    struct AccessPath {
        std::string rel;
        std::string method; // Seq Scan | Index Scan | Index Only | BitmapOr | PartitionPruned
        double cost{0.0};
        std::uint64_t rows{0};
        std::vector<std::string> used_indexes;      // for BitmapOr
        std::vector<std::string> pruned_partitions; // for partition pruning reporting
    };

    struct SargInfo {
        std::size_t equality_prefix{0};
        bool has_range{false};
    };

    // Split WHERE into top-level tokens by a delimiter (e.g., AND / OR), respecting parentheses
    static std::vector<std::string> split_top_level(const std::string& s, const std::string& delim)
    {
        std::vector<std::string> out;
        int depth = 0;
        std::size_t last = 0;
        for (std::size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '(')
                depth++;
            else if (c == ')')
                depth = depth > 0 ? depth - 1 : 0;
            if (depth == 0) {
                if (i + delim.size() <= s.size() && s.compare(i, delim.size(), delim) == 0) {
                    std::string part = s.substr(last, i - last);
                    // trim
                    auto a = part.find_first_not_of(" \t");
                    auto b = part.find_last_not_of(" \t");
                    if (a != std::string::npos)
                        out.push_back(part.substr(a, b - a + 1));
                    last = i + delim.size();
                }
            }
        }
        std::string part = s.substr(last);
        auto a = part.find_first_not_of(" \t");
        auto b = part.find_last_not_of(" \t");
        if (a != std::string::npos)
            out.push_back(part.substr(a, b - a + 1));
        return out;
    }

    enum class OpKind { Eq, Lt, Lte, Gt, Gte, Between, Like, In, Other };

    static OpKind detect_op_kind(const std::string& p)
    {
        std::string u = p;
        for (auto& ch : u)
            ch = (char)std::toupper((unsigned char)ch);
        if (u.find(" BETWEEN ") != std::string::npos)
            return OpKind::Between;
        if (u.find(" LIKE ") != std::string::npos)
            return OpKind::Like;
        if (u.find(" IN (") != std::string::npos)
            return OpKind::In;
        if (u.find(">=") != std::string::npos)
            return OpKind::Gte;
        if (u.find("<=") != std::string::npos)
            return OpKind::Lte;
        if (u.find(" = ") != std::string::npos)
            return OpKind::Eq;
        if (u.find(" > ") != std::string::npos)
            return OpKind::Gt;
        if (u.find(" < ") != std::string::npos)
            return OpKind::Lt;
        return OpKind::Other;
    }

    // Expression/functional index support: detect simple function wrappers like LOWER(col) or
    // EXPR(col)
    static bool extract_expr_column_token(const std::string& pred, std::string& out_col,
                                          std::string& out_func)
    {
        // naive: match FUNC(col) pattern on LHS
        std::size_t op_pos = std::string::npos;
        for (auto op : {" BETWEEN ", ">=", "<=", " = ", " > ", " < ", " LIKE ", " IN ("}) {
            auto p = pred.find(op);
            if (p != std::string::npos) {
                op_pos = p;
                break;
            }
        }
        std::string lhs = (op_pos == std::string::npos) ? pred : pred.substr(0, op_pos);
        auto lp = lhs.find('(');
        auto rp = lhs.find(')');
        if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
            out_func = lhs.substr(0, lp);
            auto inner = lhs.substr(lp + 1, rp - lp - 1);
            // trim
            auto a = inner.find_first_not_of(" \t");
            auto b = inner.find_last_not_of(" \t");
            if (a != std::string::npos)
                inner = inner.substr(a, b - a + 1);
            else
                inner.clear();
            if (!inner.empty()) {
                out_col = inner;
                return true;
            }
        }
        return false;
    }

    static std::string extract_column_token(const std::string& rel, const std::string& pred)
    {
        // naive: take first identifier or rel.col pattern on LHS of operator
        std::size_t pos = std::string::npos;
        // find operator position
        for (auto op : {" BETWEEN ", ">=", "<=", " = ", " > ", " < ", " LIKE ", " IN ("}) {
            auto p = pred.find(op);
            if (p != std::string::npos) {
                pos = p;
                break;
            }
        }
        std::string lhs = (pos == std::string::npos) ? pred : pred.substr(0, pos);
        // trim
        auto a = lhs.find_first_not_of(" \t(");
        auto b = lhs.find_last_not_of(" \t)");
        if (a == std::string::npos)
            return std::string();
        std::string tok = lhs.substr(a, b - a + 1);
        // If qualified, return unqualified col
        auto dot = tok.find_last_of('.');
        if (dot != std::string::npos) {
            std::string qualifier = tok.substr(0, dot);
            std::string col = tok.substr(dot + 1);
            (void)qualifier; // could compare to rel later
            return col;
        }
        return tok;
    }

    static SargInfo
    analyze_sargability_for_index(const std::string& rel,
                                  const std::vector<std::pair<std::string, std::string>>& keys,
                                  const std::string& where)
    {
        SargInfo si{};
        if (where.empty() || keys.empty())
            return si;
        // If OR appears at top-level, bail to no-sarg for safety
        {
            int depth = 0;
            for (std::size_t i = 0; i + 3 < where.size(); ++i) {
                char c = where[i];
                if (c == '(')
                    depth++;
                else if (c == ')')
                    depth = depth > 0 ? depth - 1 : 0;
                if (depth == 0 && (where.compare(i, 4, " OR ") == 0))
                    return si;
            }
        }
        auto conjuncts = split_top_level(where, " AND ");
        // Build map col (or expr) -> best op kind present
        std::unordered_map<std::string, OpKind> colops;
        std::unordered_map<std::string, std::string> exprfunc;
        for (auto& c : conjuncts) {
            std::string col = extract_column_token(rel, c);
            std::string func;
            std::string exprcol;
            if (col.empty() && extract_expr_column_token(c, exprcol, func)) {
                col = exprcol;
                exprfunc[col] = func;
            }
            if (col.empty())
                continue;
            OpKind ok = detect_op_kind(c);
            auto it = colops.find(col);
            if (it == colops.end())
                colops[col] = ok;
            else {
                // Prefer Eq over range if both present
                if (ok == OpKind::Eq || (ok == OpKind::In && it->second != OpKind::Eq))
                    colops[col] = ok;
            }
        }
        for (const auto& k : keys) {
            const std::string& col = k.first;
            auto it = colops.find(col);
            if (it == colops.end())
                break;
            if (it->second == OpKind::Eq || it->second == OpKind::In)
                si.equality_prefix++;
            else {
                si.has_range = true;
                break;
            }
        }
        // If no eq prefix, but first key has range, mark range
        if (si.equality_prefix == 0) {
            auto it0 = colops.find(keys[0].first);
            if (it0 != colops.end() && (it0->second == OpKind::Lt || it0->second == OpKind::Lte ||
                                        it0->second == OpKind::Gt || it0->second == OpKind::Gte ||
                                        it0->second == OpKind::Between))
                si.has_range = true;
        }
        return si;
    }

    static bool
    projections_subset_of_keys(const std::vector<std::string>& projections,
                               const std::vector<std::pair<std::string, std::string>>& keys)
    {
        if (projections.empty())
            return true;
        std::unordered_set<std::string> keyset;
        for (auto& k : keys)
            keyset.insert(k.first);
        for (auto& p : projections) {
            // allow simple FUNCTION(col) if index is functional on that col
            bool func_like =
                (p.find('(') != std::string::npos) && (p.find(')') != std::string::npos);
            std::string col = p;
            if (func_like) {
                auto lp = p.find('(');
                auto rp = p.find(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    col = p.substr(lp + 1, rp - lp - 1);
                }
            }
            auto dot = col.find_last_of('.');
            if (dot != std::string::npos)
                col = col.substr(dot + 1);
            if (!keyset.count(col))
                return false;
        }
        return true;
    }

    static bool index_satisfies_order(const std::vector<std::pair<std::string, std::string>>& keys,
                                      const std::vector<OrderItem>& order_by)
    {
        if (order_by.empty())
            return true;
        // Match prefix: key[i].first equals order_by[i] expression (unqualified ok)
        std::size_t n = std::min(keys.size(), order_by.size());
        if (n == 0)
            return false;
        for (std::size_t i = 0; i < n; ++i) {
            std::string ob = order_by[i].expression;
            if (ob.empty() && order_by[i].ordinal > 0)
                return false; // skip ordinal for now
            if (ob.find('.') != std::string::npos) {
                ob = ob.substr(ob.find_last_of('.') + 1);
            }
            if (keys[i].first != ob)
                return false;
            // direction check when provided
            std::string dir = keys[i].second;
            if (!dir.empty()) {
                bool asc = (dir == "ASC" || dir == "asc" || dir.empty());
                if (asc != order_by[i].ascending)
                    return false;
            }
        }
        return true;
    }

    static AccessPath choose_access_path_for_relation(const SelectQuery& q, const std::string& rel)
    {
        AccessPath best;
        best.rel = rel;
        IndexStats st{};
        auto& reg = stats_registry();
        auto it = reg.find(rel);
        if (it != reg.end())
            st = it->second;
        else {
            st.key_count = 1000000;
            st.ndistinct = 100000;
        }
        double sel = estimate_selectivity_from_where(q.where_expr, st.ndistinct);
        std::uint64_t base_rows = static_cast<std::uint64_t>(
            std::max(1.0, sel * (st.key_count > 0 ? double(st.key_count) : 100000.0)));
        // Seq path baseline
        AccessPath seq{rel, std::string("Seq Scan"),
                       cost_seq_scan(st.key_count ? st.key_count : 1000000),
                       st.key_count ? st.key_count : 1000000};
        seq.cost = apply_fdw_costing(rel, seq.cost, seq.rows);

        // Try catalog-driven index enumeration and sargability on key prefixes
        bool found_index = false;
        // Collect candidates to enable BitmapOr for simple OR cases
        std::vector<AccessPath> idx_candidates;
        try {
            CatalogManager cm(get_executor_db_path());
            cm.bootstrap_if_needed();
            auto soid = cm.lookup_schema_oid_by_name("public");
            auto idxs = cm.list_relation_indexes_by_name(soid, rel);
            for (auto& ix : idxs) {
                if (ix.keys.empty())
                    continue;
                SargInfo si = analyze_sargability_for_index(rel, ix.keys, q.where_expr);
                if (si.equality_prefix == 0 && !si.has_range)
                    continue;
                // estimate selectivity for this index
                double sel_idx = 1.0;
                if (si.equality_prefix >= 2 && !ix.keys.empty() && !st.multi_ndistinct.empty()) {
                    // build composite key name "col1,col2,..."
                    std::string comp;
                    for (std::size_t i = 0; i < si.equality_prefix && i < ix.keys.size(); ++i) {
                        if (i)
                            comp.push_back(',');
                        comp += ix.keys[i].first;
                    }
                    auto itnd = st.multi_ndistinct.find(comp);
                    if (itnd != st.multi_ndistinct.end() && itnd->second > 1.0) {
                        sel_idx = std::min(1.0, 1.0 / itnd->second);
                    } else {
                        for (std::size_t i = 0; i < si.equality_prefix; ++i)
                            sel_idx /= std::max(1.0, st.ndistinct);
                    }
                } else {
                    for (std::size_t i = 0; i < si.equality_prefix; ++i)
                        sel_idx /= std::max(1.0, st.ndistinct);
                }
                if (si.has_range)
                    sel_idx *= 0.1;
                sel_idx = std::max(1e-6, std::min(1.0, sel_idx));
                std::uint64_t rows_idx = static_cast<std::uint64_t>(
                    std::max(1.0, sel_idx * (st.key_count ? double(st.key_count) : 1000000.0)));
                double cost_idx =
                    cost_index_scan(sel_idx, st.ndistinct, st.key_count ? st.key_count : 1000000);
                // Covering: check keys and expressions
                bool covering = projections_subset_of_keys(q.projections, ix.keys);
                if (!covering && !ix.exprs.empty()) {
                    // Allow covering when projection references same expression text
                    for (const auto& p : q.projections) {
                        for (const auto& e : ix.exprs) {
                            if (p == e) {
                                covering = true;
                                break;
                            }
                        }
                        if (covering)
                            break;
                    }
                }
                if (covering)
                    cost_idx *= 0.8;
                if (st.is_pk)
                    cost_idx *= 0.9;
                bool order_ok = index_satisfies_order(ix.keys, q.order_by);
                if (order_ok && !q.order_by.empty())
                    cost_idx *= 0.85;
                AccessPath cand{rel,
                                covering ? std::string("Index Only") : std::string("Index Scan"),
                                cost_idx, rows_idx};
                cand.used_indexes = {ix.name};
                cand.cost = apply_fdw_costing(rel, cand.cost, cand.rows);
                if (!found_index || cand.cost < best.cost) {
                    best = cand;
                    found_index = true;
                }
                idx_candidates.push_back(cand);
            }
        } catch (...) {
        }
        // OR sargability via BitmapOr: combine top two candidates when WHERE has top-level OR
        if (!q.where_expr.empty()) {
            auto parts = split_top_level(q.where_expr, " OR ");
            if (parts.size() >= 2 && idx_candidates.size() >= 2) {
                std::sort(idx_candidates.begin(), idx_candidates.end(),
                          [](const AccessPath& a, const AccessPath& b) { return a.cost < b.cost; });
                AccessPath a = idx_candidates[0];
                AccessPath b = idx_candidates[1];
                AccessPath bitmap{rel, std::string("BitmapOr"), a.cost + b.cost * 0.9,
                                  std::min<std::uint64_t>(st.key_count, a.rows + b.rows)};
                bitmap.used_indexes = {a.used_indexes.empty() ? std::string() : a.used_indexes[0],
                                       b.used_indexes.empty() ? std::string() : b.used_indexes[0]};
                if (!found_index || bitmap.cost < best.cost) {
                    best = bitmap;
                    found_index = true;
                }
            }
        }
        // Partition pruning: explicit partition maps; parse simple constants and operators
        if (const PartitionMap* pm = partition_lookup(rel)) {
            std::string w = q.where_expr;
            std::string key = pm->key_column;
            std::vector<std::string> pruned_names;
            auto trim = [](std::string s) {
                auto a = s.find_first_not_of(" \t");
                auto b = s.find_last_not_of(" \t");
                if (a == std::string::npos)
                    return std::string();
                return s.substr(a, b - a + 1);
            };
            auto parse_const = [&](const std::string& tok) -> std::string {
                std::string t = trim(tok);
                if (!t.empty() && (t.front() == '\'' || t.front() == '"')) {
                    if (t.size() >= 2 && (t.back() == '\'' || t.back() == '"'))
                        return t.substr(1, t.size() - 2);
                }
                return t;
            };
            auto U = w;
            for (auto& c : U)
                c = (char)std::toupper((unsigned char)c);
            auto K = key;
            for (auto& c : K)
                c = (char)std::toupper((unsigned char)c);
            auto prune_by_between = [&](const std::string& a, const std::string& b) {
                for (const auto& pr : pm->ranges) {
                    if (!pr.list_values.empty())
                        continue; // skip list partitions here
                    if ((!pr.end.empty() && pr.end < a) || (!pr.start.empty() && pr.start > b))
                        pruned_names.push_back(pr.name);
                }
            };
            auto prune_by_cmp = [&](const std::string& op, const std::string& cst) {
                for (const auto& pr : pm->ranges) {
                    if (!pr.list_values.empty())
                        continue;
                    if (op == ">" || op == ">=") {
                        if (!pr.end.empty() &&
                            ((op == ">" && pr.end <= cst) || (op == ">=" && pr.end < cst)))
                            pruned_names.push_back(pr.name);
                    } else if (op == "<" || op == "<=") {
                        if (!pr.start.empty() &&
                            ((op == "<" && pr.start >= cst) || (op == "<=" && pr.start > cst)))
                            pruned_names.push_back(pr.name);
                    }
                }
            };
            auto prune_by_list_eq = [&](const std::string& val) {
                for (const auto& pr : pm->ranges) {
                    if (!pr.list_values.empty()) {
                        bool has = false;
                        for (auto& v : pr.list_values)
                            if (v == val) {
                                has = true;
                                break;
                            }
                        if (!has)
                            pruned_names.push_back(pr.name);
                    }
                }
            };
            if (!w.empty()) {
                // BETWEEN
                auto pos = U.find(" BETWEEN ");
                if (pos != std::string::npos) {
                    auto lhs = trim(w.substr(0, pos));
                    if (lhs == key) {
                        auto rest = w.substr(pos + 9);
                        auto andp = rest.find(" AND ");
                        if (andp != std::string::npos) {
                            std::string a = parse_const(rest.substr(0, andp));
                            std::string b = parse_const(rest.substr(andp + 5));
                            prune_by_between(a, b);
                        }
                    }
                }
                // Comparators
                for (auto op :
                     {std::string(">="), std::string("<="), std::string(">"), std::string("<")}) {
                    auto p = U.find(" " + op + " ");
                    if (p != std::string::npos) {
                        auto lhs = trim(w.substr(0, p));
                        if (lhs == key) {
                            std::string rhs = parse_const(w.substr(p + op.size() + 2));
                            prune_by_cmp(op, rhs);
                            break;
                        }
                    }
                }
                // Equality for list partitions
                auto pe = U.find(" = ");
                if (pe != std::string::npos) {
                    auto lhs = trim(w.substr(0, pe));
                    if (lhs == key) {
                        std::string rhs = parse_const(w.substr(pe + 3));
                        prune_by_list_eq(rhs);
                    }
                }
            }
            if (!pruned_names.empty()) {
                AccessPath pruned = best;
                pruned.method = "PartitionPruned";
                pruned.pruned_partitions = pruned_names;
                pruned.rows = std::max<std::uint64_t>(
                    1, best.rows - (best.rows * pruned_names.size()) /
                                       std::max<size_t>(1, pm->ranges.size()));
                pruned.cost = std::max(
                    1.0, best.cost * (0.6 + 0.4 * (double(pruned.rows) /
                                                   double(std::max<std::uint64_t>(1, best.rows)))));
                if (pruned.cost < best.cost)
                    best = pruned;
            }
        }
        if (!found_index)
            best = seq;
        else if (best.cost > seq.cost)
            best = seq;
        return best;
    }

    static PlanChoice plan_select_basic(const SelectQuery& q)
    {
        PlanChoice pc{};
        if (q.from_items.empty()) {
            pc.method = "result";
            pc.cost = 1.0;
            pc.est_rows = 1;
            pc.order_satisfied = true;
            pc.index_only = true;
            return pc;
        }
        std::string rel =
            q.from_items.front().table.empty() ? q.from_table : q.from_items.front().table;
        pc.relation = rel;
        // Lookup stats
        IndexStats st{};
        auto& reg = stats_registry();
        auto it = reg.find(rel);
        if (it != reg.end())
            st = it->second;
        else {
            st.key_count = 1000000;
            st.leaf_pages = 4096;
            st.branch_pages = 64;
            st.ndistinct = 100000;
            st.correlation = 0.3;
        }
        double sel = estimate_selectivity_from_where(q.where_expr, st.ndistinct);
        pc.selectivity = sel;
        pc.est_rows = static_cast<std::uint64_t>(
            std::max(1.0, sel * (st.key_count > 0 ? double(st.key_count) : 100000.0)));
        pc.index_only = is_index_only_candidate(q);
        pc.order_satisfied = order_by_satisfied(q.order_by);
        // Choose method
        if (!q.where_expr.empty()) {
            pc.method = pc.index_only ? "idx_only" : "idx_scan";
            pc.cost = cost_index_scan(sel, st.ndistinct, st.key_count ? st.key_count : 1000000);
            pc.index_name = rel + "_idx";
        } else {
            pc.method = "seq_scan";
            pc.cost = cost_seq_scan(st.key_count ? st.key_count : 1000000);
        }
        return pc;
    }

    std::string explain_select_plan(const SelectQuery& q, bool analyze)
    {
        PlanChoice pc = plan_select_basic(q);
        char buf[256];
        std::string method =
            (pc.method == "idx_only"
                 ? "Index Only Scan"
                 : (pc.method == "idx_scan" ? "Index Scan"
                                            : (pc.method == "seq_scan" ? "Seq Scan" : "Result")));
        std::snprintf(buf, sizeof(buf), "%s on %s%s  cost=%.2f rows=%llu%s%s", method.c_str(),
                      pc.relation.empty() ? "<unknown>" : pc.relation.c_str(),
                      (pc.method == "idx_scan" || pc.method == "idx_only") && !pc.index_name.empty()
                          ? (std::string(" using ") + pc.index_name).c_str()
                          : "",
                      pc.cost, static_cast<unsigned long long>(pc.est_rows),
                      pc.order_satisfied ? " order_satisfied" : "",
                      pc.index_only ? " index_only" : "");
        std::string out = buf;
        if (analyze) {
            // Append a toy timing estimate
            char tb[64];
            std::snprintf(tb, sizeof(tb), "  (analyze est=%.2f ms)", pc.cost * 0.5);
            out += tb;
        }
        return out;
    }

    // Optimizer cache invalidation (simple textual-plan cache)
    void invalidate_optimizer_cache();
    // Lightweight optimizer façade: multi-relation with DP/greedy + formatted plan
    std::string optimize_select_plan(const SelectQuery& q, bool analyze)
    {
        // Plan cache (very simple): key is minimal SELECT signature
        auto& plan_cache = get_plan_cache();
        auto make_key = [&](const SelectQuery& qk) {
            // crude key: first projection + first from + where hash
            std::string k;
            if (!qk.projections.empty())
                k += qk.projections[0];
            if (!qk.from_items.empty())
                k += "|" + qk.from_items[0].table;
            k += "|" + qk.where_expr;
            // parameter bucket from executor, when available
            try {
                // weak coupling: reuse prepared handle bucket if set; otherwise default
                // In this toy integration we cannot see the handle; append a heuristic bucket from
                // selectivity
                double nd = 1000.0;
                if (!qk.from_items.empty()) {
                    std::string rel =
                        qk.from_items[0].table.empty() ? qk.from_table : qk.from_items[0].table;
                    auto itst = stats_registry().find(rel);
                    if (itst != stats_registry().end() && itst->second.ndistinct > 0)
                        nd = itst->second.ndistinct;
                }
                double sel = estimate_selectivity_from_where(qk.where_expr, nd);
                std::string bucket = (sel <= 0.05) ? "LOW" : ((sel >= 0.4) ? "HIGH" : "MID");
                k += "|B=" + bucket;
                // include a stats epoch to force replan on stats changes (use first relation)
                if (!qk.from_items.empty()) {
                    std::string rel =
                        qk.from_items[0].table.empty() ? qk.from_table : qk.from_items[0].table;
                    std::uint64_t epoch = stats_relation_epoch(rel);
                    k += "|E=" + std::to_string(epoch);
                }
            } catch (...) {
            }
            return k;
        };
        std::string key = make_key(q);
        if (const std::string* cached = plan_cache.get(key))
            return *cached;

        if (q.from_items.size() <= 1) {
            std::string s = explain_select_plan(q, analyze);
            plan_cache.set(key, s);
            return s;
        }
        // Build base access paths
        std::vector<AccessPath> paths;
        paths.reserve(q.from_items.size());
        for (size_t i = 0; i < q.from_items.size(); ++i) {
            std::string rel =
                (i == 0 && q.from_items[i].table.empty()) ? q.from_table : q.from_items[i].table;
            paths.push_back(choose_access_path_for_relation(q, rel));
        }
        const size_t n = paths.size();
        // DP join ordering for small n (<=5), else greedy by rows
        std::vector<size_t> best_order;
        bool use_dp = n <= 5;
        if (use_dp) {
            const int N = static_cast<int>(n);
            const int FULL = 1 << N;
            struct State {
                double cost;
                double rows;
                std::vector<int> order;
            };
            std::vector<State> dp(FULL, {1e300, 0.0, {}});
            for (int i = 0; i < N; ++i) {
                State s;
                s.cost = paths[i].cost;
                s.rows = static_cast<double>(paths[i].rows);
                s.order = {i};
                dp[1 << i] = s;
            }
            auto join_selectivity = [&](double rowsA, double rowsB) {
                (void)rowsA;
                (void)rowsB;
                bool has_join = !q.joins.empty();
                return has_join ? 0.1 : 1.0;
            };
            for (int mask = 1; mask < FULL; ++mask) {
                if (dp[mask].cost >= 1e300)
                    continue;
                for (int nxt = 0; nxt < N; ++nxt) {
                    if (mask & (1 << nxt))
                        continue;
                    int nmask = mask | (1 << nxt);
                    double sel =
                        join_selectivity(dp[mask].rows, static_cast<double>(paths[nxt].rows));
                    double join_rows =
                        std::max(1.0, dp[mask].rows * static_cast<double>(paths[nxt].rows) * sel);
                    double join_cost =
                        dp[mask].cost +
                        dp[mask].rows * std::log2(static_cast<double>(paths[nxt].rows) + 1.0) +
                        paths[nxt].cost;
                    if (join_cost < dp[nmask].cost) {
                        State s = dp[mask];
                        s.cost = join_cost;
                        s.rows = join_rows;
                        s.order.push_back(nxt);
                        dp[nmask] = std::move(s);
                    }
                }
            }
            State best = dp[FULL - 1];
            best_order.assign(best.order.begin(), best.order.end());
        } else {
            best_order.resize(n);
            std::iota(best_order.begin(), best_order.end(), 0);
            std::sort(best_order.begin(), best_order.end(),
                      [&](size_t a, size_t b) { return paths[a].rows < paths[b].rows; });
        }
        // Join method: extend with merge-join possibility when both inputs ordered on join keys
        bool use_hash = (!q.joins.empty() && n >= 2 && (paths[best_order.back()].rows > 100000));
        bool use_merge = false;
        if (!q.joins.empty() && n >= 2 && !q.order_by.empty()) {
            // toy: if both chosen base paths claim order satisfied, prefer merge over hash
            use_merge = true;
            use_hash = false;
        }
        auto hints2 = get_optimizer_hints();
        if (hints2.force_merge_join) {
            use_merge = true;
            use_hash = false;
        }
        if (hints2.force_hash_join) {
            use_hash = true;
            use_merge = false;
        }
        // Detect semi/anti join pattern from WHERE (EXISTS/NOT EXISTS/IN/NOT IN)
        bool is_semi = false, is_anti = false;
        if (!q.where_expr.empty()) {
            std::string u = q.where_expr;
            for (auto& c : u)
                c = (char)std::toupper((unsigned char)c);
            if (u.find(" NOT EXISTS ") != std::string::npos ||
                u.find(" NOT IN ") != std::string::npos)
                is_anti = true;
            else if (u.find(" EXISTS ") != std::string::npos ||
                     u.find(" IN (") != std::string::npos)
                is_semi = true;
        }
        // Honor hints
        auto hints = get_optimizer_hints();
        if (hints.force_nested_loop)
            use_hash = false;
        if (hints.force_hash_join)
            use_hash = true;
        // Render plan and compute join node cost/rows along chosen order. Include semi/anti/outer
        // adjustments.
        std::string out = use_hash ? "Hash Join Plan: "
                                   : (use_merge ? "Merge Join Plan: " : "Nested Loop Plan: ");
        if (is_semi)
            out = std::string("Semi ") + out;
        if (is_anti)
            out = std::string("Anti ") + out;
        double cum_rows = 0.0;
        double join_cost_accum = 0.0;
        double mem_bytes = 0.0;
        for (size_t i = 0; i < best_order.size(); ++i) {
            const auto& p = paths[best_order[i]];
            if (i)
                out += " -> ";
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s on %s rows=%llu cost=%.2f", p.method.c_str(),
                          p.rel.c_str(), (unsigned long long)p.rows, p.cost);
            out += buf;
            if (p.method == "BitmapOr" && !p.used_indexes.empty()) {
                out +=
                    " using(" + (p.used_indexes.size() > 0 ? p.used_indexes[0] : std::string(""));
                if (p.used_indexes.size() > 1)
                    out += "+" + p.used_indexes[1];
                out += ")";
            }
            if (p.method == "PartitionPruned" && !p.pruned_partitions.empty()) {
                out += " pruned=" + std::to_string(p.pruned_partitions.size());
            }
            if (i == 0) {
                cum_rows = static_cast<double>(p.rows);
                join_cost_accum += p.cost;
            } else {
                // Join cost accumulation per method
                if (use_hash) {
                    join_cost_accum += cum_rows + p.rows + p.cost; // build+probe toy cost
                    mem_bytes += std::max(1.0, cum_rows) * 32.0;   // rough hash table bytes
                } else if (use_merge) {
                    join_cost_accum +=
                        cum_rows * 0.6 + p.rows * 0.6 + p.cost; // linear-ish merge scan
                } else {
                    join_cost_accum +=
                        cum_rows * std::log2(static_cast<double>(p.rows) + 1.0) + p.cost; // NLJ-ish
                }
                // crude selectivity for join kinds: inner=0.1, left=0.2, semi=0.05, anti=0.05,
                // full=0.3
                double base_sel = 0.1;
                if (is_semi)
                    base_sel = 0.05;
                else if (is_anti)
                    base_sel = 0.05;
                if (!q.joins.empty()) {
                    auto jt = q.joins[0].type; // simplification
                    switch (jt) {
                    case JoinType::Left:
                        base_sel = 0.2;
                        break;
                    case JoinType::Full:
                        base_sel = 0.3;
                        break;
                    case JoinType::Right:
                        base_sel = 0.2;
                        break;
                    default:
                        base_sel = 0.1;
                        break;
                    }
                }
                cum_rows = std::max(1.0, cum_rows * static_cast<double>(p.rows) * base_sel);
            }
        }
        // Emit join node summary
        if (best_order.size() >= 2) {
            char jbuf[128];
            std::snprintf(jbuf, sizeof(jbuf), " | Join rows=%llu cost=%.2f",
                          (unsigned long long)cum_rows, join_cost_accum);
            out += jbuf;
        }
        // Top operations: Sort / HashAgg (toy costs + spill flags)
        if (!q.order_by.empty()) {
            double sort_cost = cum_rows * std::log2(cum_rows + 1.0) * 0.5;
            bool spill = cum_rows > 200000.0;
            char sbuf[128];
            std::snprintf(sbuf, sizeof(sbuf), " | Sort rows=%llu cost=%.2f%s",
                          (unsigned long long)cum_rows, sort_cost, spill ? " spill" : "");
            out += sbuf;
        }
        if (!q.group_by.empty()) {
            double groups = std::max(1.0, std::min(cum_rows, 10000.0));
            double agg_cost = cum_rows * 0.2;
            bool spill = cum_rows > 300000.0;
            char abuf[128];
            std::snprintf(abuf, sizeof(abuf), " | HashAgg groups=%llu cost=%.2f%s",
                          (unsigned long long)groups, agg_cost, spill ? " spill" : "");
            out += abuf;
        }
        // Append final est rows and memory footprint estimate
        {
            char tail[80];
            std::snprintf(tail, sizeof(tail), "  final_rows=%llu mem_est_bytes=%llu",
                          (unsigned long long)cum_rows, (unsigned long long)mem_bytes);
            out += tail;
        }
        plan_cache.set(key, out);
        // Tag with relations involved for finer invalidation
        std::vector<std::string> rels;
        for (size_t i = 0; i < q.from_items.size(); ++i) {
            std::string rel =
                (i == 0 && q.from_items[i].table.empty()) ? q.from_table : q.from_items[i].table;
            rels.push_back(rel);
        }
        // Stats epoch from first relation
        std::uint64_t epoch = 0;
        if (!rels.empty())
            epoch = stats_relation_epoch(rels[0]);
        get_plan_cache().tag_relations(key, rels, epoch);
        return out;
    }

    static void append_indent(std::string& s, int n)
    {
        for (int i = 0; i < n; ++i)
            s.push_back(' ');
    }

    std::vector<std::string> optimize_select_plan_multiline(const SelectQuery& q, bool analyze)
    {
        std::vector<std::string> lines;
        // Reuse single-line builder logic, but break into a small tree:
        // Join node at root, children are base scans, with optional Sort/HashAgg nodes above
        // Build access paths as in optimize_select_plan
        std::vector<AccessPath> paths;
        std::string from0 =
            q.from_items.empty()
                ? q.from_table
                : (q.from_items[0].table.empty() ? q.from_table : q.from_items[0].table);
        for (size_t i = 0; i < q.from_items.size(); ++i) {
            std::string rel =
                (i == 0 && q.from_items[i].table.empty()) ? q.from_table : q.from_items[i].table;
            paths.push_back(choose_access_path_for_relation(q, rel));
        }
        if (paths.empty()) {
            lines.push_back("Result  cost=1.00 rows=1");
            return lines;
        }
        std::vector<size_t> order(paths.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](size_t a, size_t b) { return paths[a].rows < paths[b].rows; });
        bool use_hash =
            (!q.joins.empty() && paths.size() >= 2 && (paths[order.back()].rows > 100000));
        // Root join node
        double cum_rows = 0.0;
        double join_cost = 0.0;
        {
            for (size_t i = 0; i < order.size(); ++i) {
                const auto& p = paths[order[i]];
                if (i == 0) {
                    cum_rows = static_cast<double>(p.rows);
                    join_cost += p.cost;
                } else {
                    join_cost += cum_rows * std::log2(static_cast<double>(p.rows) + 1.0) + p.cost;
                    cum_rows = std::max(1.0, cum_rows * static_cast<double>(p.rows) *
                                                 (!q.joins.empty() ? 0.1 : 1.0));
                }
            }
            char hdr[160];
            std::snprintf(hdr, sizeof(hdr), "%sJoin  rows=%llu cost=%.2f",
                          use_hash ? "Hash " : "Nested Loop ", (unsigned long long)cum_rows,
                          join_cost);
            lines.emplace_back(hdr);
        }
        // Child scans
        for (size_t i = 0; i < order.size(); ++i) {
            const auto& p = paths[order[i]];
            std::string ln;
            append_indent(ln, 2);
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s on %s  rows=%llu cost=%.2f", p.method.c_str(),
                          p.rel.c_str(), (unsigned long long)p.rows, p.cost);
            ln += buf;
            lines.emplace_back(std::move(ln));
        }
        if (analyze) {
            // If analyze requested, append placeholders for actuals per node; the executor fills a
            // final summary In a fuller integration, these would be fed from per-node metrics
        }
        // Top ops above join
        if (!q.group_by.empty()) {
            std::string ln;
            append_indent(ln, 2);
            double groups = std::max(1.0, std::min(cum_rows, 10000.0));
            double agg_cost = cum_rows * 0.2;
            bool spill = cum_rows > 300000.0;
            char abuf[160];
            std::snprintf(abuf, sizeof(abuf), "HashAgg  groups=%llu cost=%.2f%s",
                          (unsigned long long)groups, agg_cost, spill ? " spill" : "");
            ln += abuf;
            lines.emplace_back(std::move(ln));
        }
        if (!q.order_by.empty()) {
            std::string ln;
            append_indent(ln, 2);
            double sort_cost = cum_rows * std::log2(cum_rows + 1.0) * 0.5;
            bool spill = cum_rows > 200000.0;
            char sbuf[160];
            std::snprintf(sbuf, sizeof(sbuf), "Sort  rows=%llu cost=%.2f%s",
                          (unsigned long long)cum_rows, sort_cost, spill ? " spill" : "");
            ln += sbuf;
            lines.emplace_back(std::move(ln));
        }
        return lines;
    }
    int choose_two_relation_join_order(const SelectQuery& q)
    {
        if (q.from_items.size() < 2)
            return 0;
        std::string relA = q.from_items[0].table.empty() ? q.from_table : q.from_items[0].table;
        std::string relB = q.from_items[1].table;
        std::uint64_t rowsA = estimate_rows_for_relation(relA, q.where_expr);
        std::uint64_t rowsB = estimate_rows_for_relation(relB, q.where_expr);
        return (rowsB < rowsA) ? 1 : 0;
    }

    void invalidate_optimizer_cache()
    {
        get_plan_cache().clear();
    }

    void invalidate_optimizer_cache_for_relation(const std::string& relation)
    {
        // coarse approach: drop entire cache when relation appears in key
        // (LRU lacks iteration API; rebuild by clearing for now)
        (void)relation;
        get_plan_cache().clear();
    }

} // namespace scratchbird::engine
