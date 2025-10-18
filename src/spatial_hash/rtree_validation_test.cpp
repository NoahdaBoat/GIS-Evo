#include "rtree.hpp"
#include "simple_spatial_index.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

namespace gisevo::rtree_validation {

struct ValidationStats {
    std::size_t scenarios{};
    std::size_t subtests{};
    std::size_t failures{};
};

namespace {

using Box = BoundingBox;
using IndexValue = std::uint32_t;
using RTreeIndex = RTree<IndexValue>;
using VectorIndex = SimpleSpatialIndex<IndexValue>;

bool compare_results(std::vector<IndexValue> lhs, std::vector<IndexValue> rhs) {
    std::sort(lhs.begin(), lhs.end());
    std::sort(rhs.begin(), rhs.end());
    return lhs == rhs;
}

template <typename Generator>
std::vector<std::pair<IndexValue, Box>> generate_random_items(std::size_t count, Generator& rng) {
    std::uniform_real_distribution<double> coord_dist(-500.0, 500.0);
    std::uniform_real_distribution<double> span_dist(0.01, 10.0);

    std::vector<std::pair<IndexValue, Box>> items;
    items.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const double min_x = coord_dist(rng);
        const double min_y = coord_dist(rng);
        const double span_x = span_dist(rng);
        const double span_y = span_dist(rng);
        items.emplace_back(static_cast<IndexValue>(i), Box{min_x, min_y, min_x + span_x, min_y + span_y});
    }

    return items;
}

template <typename Generator>
std::vector<Box> generate_random_queries(std::size_t count, Generator& rng) {
    std::uniform_real_distribution<double> center_dist(-600.0, 600.0);
    std::uniform_real_distribution<double> span_dist(0.1, 60.0);

    std::vector<Box> queries;
    queries.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const double cx = center_dist(rng);
        const double cy = center_dist(rng);
        const double half = span_dist(rng) * 0.5;
        queries.emplace_back(Box{cx - half, cy - half, cx + half, cy + half});
    }

    return queries;
}

template <typename Collection>
void populate_indexes(Collection&& items, RTreeIndex& tree, VectorIndex& fallback) {
    for (const auto& [id, bounds] : items) {
        tree.insert(id, bounds);
        fallback.insert(id, bounds);
    }
}

bool run_basic_scenario() {
    RTreeIndex tree;
    VectorIndex fallback;

    const std::array<Box, 4> boxes = {
        Box{0.0, 0.0, 1.0, 1.0},
        Box{-1.0, -1.0, -0.8, -0.6},
        Box{5.0, 5.0, 10.0, 10.0},
        Box{-2.0, -2.0, 2.0, 2.0}
    };

    for (std::size_t i = 0; i < boxes.size(); ++i) {
        tree.insert(static_cast<IndexValue>(i), boxes[i]);
        fallback.insert(static_cast<IndexValue>(i), boxes[i]);
    }

    const std::array<Box, 4> queries = {
        Box{-0.5, -0.5, 0.5, 0.5},
        Box{-10.0, -10.0, -0.7, -0.5},
        Box{4.0, 4.0, 6.0, 6.0},
        Box{-3.0, -3.0, 3.0, 3.0}
    };

    for (const auto& query : queries) {
        if (!compare_results(tree.query(query), fallback.query(query))) {
            std::cerr << "Basic scenario mismatch" << std::endl;
            return false;
        }
    }

    return true;
}

bool run_duplicate_bounds_scenario() {
    RTreeIndex tree;
    VectorIndex fallback;

    const Box duplicate{10.0, 10.0, 15.0, 15.0};
    for (IndexValue i = 0; i < 20; ++i) {
        tree.insert(i, duplicate);
        fallback.insert(i, duplicate);
    }

    const auto query_bounds = Box{9.0, 9.0, 16.0, 16.0};
    return compare_results(tree.query(query_bounds), fallback.query(query_bounds));
}

bool run_random_scenario(std::size_t items, std::size_t queries, std::uint64_t seed) {
    std::mt19937_64 rng(seed);

    RTreeIndex tree;
    VectorIndex fallback;

    const auto generated_items = generate_random_items(items, rng);
    populate_indexes(generated_items, tree, fallback);

    const auto generated_queries = generate_random_queries(queries, rng);
    for (const auto& query : generated_queries) {
        if (!compare_results(tree.query(query), fallback.query(query))) {
            std::cerr << "Random scenario mismatch at seed " << seed << std::endl;
            return false;
        }
    }

    return true;
}

bool run_split_stress_scenario(std::size_t items, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    RTreeIndex tree;

    const auto generated_items = generate_random_items(items, rng);
    for (const auto& [id, bounds] : generated_items) {
        tree.insert(id, bounds);
    }

    return tree.size() == items;
}

} // namespace

ValidationStats run_all() {
    ValidationStats stats{};

    const std::vector<std::pair<const char*, bool (*)()>> scenarios = {
        {"basic", &run_basic_scenario},
        {"duplicate_bounds", &run_duplicate_bounds_scenario},
        {"random_small", [] { return run_random_scenario(200, 200, 1337); }},
        {"random_medium", [] { return run_random_scenario(2'000, 500, 4242); }},
        {"random_large", [] { return run_random_scenario(20'000, 250, 98765); }},
        {"split_stress", [] { return run_split_stress_scenario(50'000, 56789); }}
    };

    for (const auto& [name, scenario] : scenarios) {
        ++stats.scenarios;
        if (!scenario()) {
            ++stats.failures;
            std::cerr << "Scenario failed: " << name << std::endl;
        } else {
            std::cout << "Scenario passed: " << name << std::endl;
        }
    }

    return stats;
}

} // namespace gisevo::rtree_validation

int main() {
    const auto stats = gisevo::rtree_validation::run_all();
    if (stats.failures > 0) {
        std::cerr << stats.failures << " scenario(s) failed" << std::endl;
        return 1;
    }

    std::cout << "All " << stats.scenarios << " scenarios passed" << std::endl;
    return 0;
}
