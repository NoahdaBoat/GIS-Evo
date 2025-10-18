#include "../include/bounding_box.hpp"
#include "rtree.hpp"
#include "simple_spatial_index.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace gisevo::benchmarks {

struct BenchmarkConfig {
    std::string label;
    std::size_t item_count;
    std::size_t query_count;
    double region_span;
    double query_span;
};

struct BenchmarkResult {
    std::string label;
    std::string index_name;
    std::size_t item_count{};
    std::size_t query_count{};
    double build_ms{};
    double query_ms{};
    double avg_results{};
    std::size_t max_results{};
};

namespace detail {

constexpr double kWorldMin = -1000.0;
constexpr double kWorldMax = 1000.0;

struct ItemGenerator {
    std::mt19937_64 rng;
    std::uniform_real_distribution<double> coord_dist;
    std::uniform_real_distribution<double> span_dist;

    explicit ItemGenerator(std::uint64_t seed)
        : rng(seed)
        , coord_dist(kWorldMin, kWorldMax)
        , span_dist(5.0, 25.0) {}

    BoundingBox next_box() {
        const double min_x = coord_dist(rng);
        const double min_y = coord_dist(rng);
        const double span_x = span_dist(rng);
        const double span_y = span_dist(rng);
        return {min_x, min_y, min_x + span_x, min_y + span_y};
    }

    BoundingBox next_query(double center_span, double window_span) {
        std::uniform_real_distribution<double> center_dist(-center_span, center_span);
        const double cx = center_dist(rng);
        const double cy = center_dist(rng);
        const double half = window_span * 0.5;
        return {cx - half, cy - half, cx + half, cy + half};
    }
};

template <typename Index>
struct IndexTraits;

template <typename T>
struct IndexTraits<SimpleSpatialIndex<T>> {
    static SimpleSpatialIndex<T> create() { return {}; }
    static constexpr const char* name() { return "SimpleSpatialIndex"; }
};

template <typename T>
struct IndexTraits<RTree<T>> {
    static RTree<T> create() { return {}; }
    static constexpr const char* name() { return "RTree"; }
};

template <typename Index>
BenchmarkResult execute(const BenchmarkConfig& cfg, ItemGenerator& generator,
                        std::vector<BoundingBox>& boxes,
                        std::vector<BoundingBox>& queries,
                        std::vector<std::size_t>& query_hit_counts) {
    boxes.clear();
    queries.clear();

    boxes.reserve(cfg.item_count);
    queries.reserve(cfg.query_count);

    for (std::size_t i = 0; i < cfg.item_count; ++i) {
        boxes.push_back(generator.next_box());
    }

    for (std::size_t q = 0; q < cfg.query_count; ++q) {
        queries.push_back(generator.next_query(cfg.region_span, cfg.query_span));
    }

    Index index = IndexTraits<Index>::create();

    const auto build_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < cfg.item_count; ++i) {
        index.insert(i, boxes[i]);
    }
    const auto build_end = std::chrono::steady_clock::now();

    query_hit_counts.assign(cfg.query_count, 0);

    const auto query_start = std::chrono::steady_clock::now();
    for (std::size_t q = 0; q < cfg.query_count; ++q) {
        const auto hits = index.query(queries[q]);
        query_hit_counts[q] = hits.size();
    }
    const auto query_end = std::chrono::steady_clock::now();

    const double build_ms = std::chrono::duration<double, std::milli>(build_end - build_start).count();
    const double query_ms = std::chrono::duration<double, std::milli>(query_end - query_start).count();

    const auto [min_it, max_it] = std::minmax_element(query_hit_counts.begin(), query_hit_counts.end());

    const double avg_hits = std::accumulate(query_hit_counts.begin(), query_hit_counts.end(), 0.0)
        / static_cast<double>(cfg.query_count);

    return {
        cfg.label,
        IndexTraits<Index>::name(),
        cfg.item_count,
        cfg.query_count,
        build_ms,
        query_ms,
        avg_hits,
        *max_it
    };
}

} // namespace detail

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(std::uint64_t seed = std::random_device{}())
        : generator_(seed) {}

    void add_config(BenchmarkConfig cfg) { configs_.push_back(std::move(cfg)); }

    [[nodiscard]] std::vector<BenchmarkResult> run() {
        std::vector<BenchmarkResult> results;
        std::vector<BoundingBox> boxes;
        std::vector<BoundingBox> queries;
        std::vector<std::size_t> query_hits;

        for (const auto& cfg : configs_) {
            results.push_back(detail::execute<SimpleSpatialIndex<std::size_t>>(cfg, generator_, boxes, queries, query_hits));
            results.push_back(detail::execute<RTree<std::size_t>>(cfg, generator_, boxes, queries, query_hits));
        }

        return results;
    }

private:
    std::vector<BenchmarkConfig> configs_;
    detail::ItemGenerator generator_;
};

void print_results(const std::vector<BenchmarkResult>& results) {
    auto header = [] {
        std::cout << std::left
                  << std::setw(12) << "Scenario"
                  << std::setw(22) << "Index"
                  << std::setw(12) << "Items"
                  << std::setw(12) << "Queries"
                  << std::setw(12) << "Build(ms)"
                  << std::setw(12) << "Query(ms)"
                  << std::setw(14) << "Avg Hits"
                  << std::setw(14) << "Max Hits"
                  << '\n';
        std::cout << std::string(110, '-') << '\n';
    };

    header();
    for (const auto& result : results) {
        std::cout << std::left
                  << std::setw(12) << result.label
                  << std::setw(22) << result.index_name
                  << std::setw(12) << result.item_count
                  << std::setw(12) << result.query_count
                  << std::setw(12) << std::fixed << std::setprecision(3) << result.build_ms
                  << std::setw(12) << std::fixed << std::setprecision(3) << result.query_ms
                  << std::setw(14) << std::fixed << std::setprecision(2) << result.avg_results
                  << std::setw(14) << result.max_results
                  << '\n';
    }
}

} // namespace gisevo::benchmarks

int main() {
    using gisevo::benchmarks::BenchmarkConfig;
    gisevo::benchmarks::BenchmarkRunner runner;

    runner.add_config({"small", 5'000, 2'000, 250.0, 40.0});
    runner.add_config({"medium", 50'000, 5'000, 400.0, 80.0});
    runner.add_config({"large", 250'000, 8'000, 600.0, 120.0});

    const auto results = runner.run();
    gisevo::benchmarks::print_results(results);

    return 0;
}
