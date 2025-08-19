#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_btree.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace scratchbird::engine;

enum class Dist { Uniform, Zipf, Skew };

static std::vector<std::string> make_keys(std::size_t n, Dist d)
{
    std::vector<std::string> out;
    out.reserve(n);
    std::mt19937_64 rng(12345);
    if (d == Dist::Uniform) {
        std::uniform_int_distribution<std::uint64_t> U(0, n * 10);
        for (std::size_t i = 0; i < n; ++i)
            out.push_back(std::to_string(U(rng)));
    } else if (d == Dist::Skew) {
        std::uniform_int_distribution<std::uint64_t> U(0, n / 10 + 1);
        for (std::size_t i = 0; i < n; ++i)
            out.push_back(std::to_string(U(rng)));
    } else {
        // Zipf-like via discrete_distribution with weights 1/(k+1)
        std::vector<double> w(n);
        for (std::size_t i = 0; i < n; ++i)
            w[i] = 1.0 / (i + 1);
        std::discrete_distribution<int> Z(w.begin(), w.end());
        std::vector<std::string> z;
        z.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            z.push_back(std::to_string(Z(rng)));
        out.swap(z);
    }
    return out;
}

static double ms_since(std::chrono::steady_clock::time_point t0)
{
    using namespace std::chrono;
    return duration_cast<duration<double, std::milli>>(steady_clock::now() + milliseconds(0) - t0)
        .count();
}

int main(int argc, char** argv)
{
    if (argc < 6) {
        std::fprintf(
            stderr,
            "Usage: %s <base_path> <page_size> <N> <uniform|zipf|skew> <point|range|insert|delete> "
            "[fillfactor=0.7] [prefetch_pages=0] [split=even|left|right]\n",
            argv[0]);
        return 1;
    }
    std::string base = argv[1];
    std::uint32_t page_size = static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10));
    std::size_t N = static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10));
    std::string dist_s = argv[4];
    std::string op = argv[5];
    Dist d = dist_s == "uniform" ? Dist::Uniform : (dist_s == "skew" ? Dist::Skew : Dist::Zipf);

    FileMap::Layout layout{};
    layout.page_size = page_size;
    layout.pages_per_segment = 4096;
    layout.options = FileOptions{};
    FileMap fmap(layout);
    auto slash = base.find_last_of('/');
    std::string dir = (slash == std::string::npos) ? std::string(".") : base.substr(0, slash);
    std::string stem = (slash == std::string::npos) ? base : base.substr(slash + 1);
    fmap.set_base_path(dir, stem);

    BTreeIndex idx(std::move(fmap), page_size, false);
    idx.create_empty();

    BTreeTunables tun{};
    tun.prefetch_horizon_pages = 8;
    tun.fillfactor = 0.7;
    if (argc > 6)
        tun.fillfactor = std::max(0.5, std::min(0.95, std::atof(argv[6])));
    if (argc > 7)
        tun.prefetch_horizon_pages = static_cast<std::uint32_t>(std::strtoul(argv[7], nullptr, 10));
    if (argc > 8) {
        std::string sp = argv[8];
        if (sp == "left")
            tun.split_policy = BTreeTunables::SplitPolicy::LeftBiased;
        else if (sp == "right")
            tun.split_policy = BTreeTunables::SplitPolicy::RightBiased;
        else
            tun.split_policy = BTreeTunables::SplitPolicy::Even;
    }
    idx.set_tunables(tun);

    auto keys = make_keys(N, d);
    std::string err;

    if (op == "insert") {
        auto t0 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < keys.size(); ++i) {
            idx.insert(keys[i], static_cast<std::uint64_t>(i + 1), err);
        }
        std::printf("insert: N=%zu ms=%.2f\n", N, ms_since(t0));
    } else if (op == "point") {
        // Preload with inserts
        for (std::size_t i = 0; i < keys.size(); ++i)
            idx.insert(keys[i], static_cast<std::uint64_t>(i + 1), err);
        std::vector<std::uint64_t> out;
        out.reserve(8);
        auto t0 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            out.clear();
            idx.search_equal(keys[i % keys.size()], out);
        }
        std::printf("point: N=%zu ms=%.2f\n", N, ms_since(t0));
    } else if (op == "range") {
        for (std::size_t i = 0; i < keys.size(); ++i)
            idx.insert(keys[i], static_cast<std::uint64_t>(i + 1), err);
        std::vector<std::pair<std::string, std::uint64_t>> out;
        out.reserve(128);
        auto t0 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            out.clear();
            idx.search_range("", true, "zzzzzz", true, out);
        }
        std::printf("range: N=%zu ms=%.2f\n", N, ms_since(t0));
    } else if (op == "delete") {
        for (std::size_t i = 0; i < keys.size(); ++i)
            idx.insert(keys[i], static_cast<std::uint64_t>(i + 1), err);
        auto t0 = std::chrono::steady_clock::now();
        std::size_t removed = 0;
        for (std::size_t i = 0; i < keys.size(); ++i)
            removed += idx.erase_equal(keys[i], err);
        std::printf("delete: N=%zu removed=%zu ms=%.2f\n", N, removed, ms_since(t0));
    }

    return 0;
}
