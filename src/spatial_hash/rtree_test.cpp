#include "rtree.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace gisevo {

namespace rtree_tests {

using Tree = RTree<int>;
using Box = BoundingBox;

bool test_empty_tree() {
    Tree tree;
    auto results = tree.query(Box(-10, -10, 10, 10));
    return results.empty() && tree.size() == 0;
}

bool test_single_insert() {
    Tree tree;
    tree.insert(42, Box(0, 0, 1, 1));

    auto results = tree.query(Box(-1, -1, 2, 2));
    return results.size() == 1 && results.front() == 42 && tree.size() == 1;
}

bool test_clear() {
    Tree tree;
    tree.insert(1, Box(-1, -1, 1, 1));
    tree.clear();

    return tree.size() == 0 && tree.query(Box(-2, -2, 2, 2)).empty();
}

bool test_bulk_insert_full_query() {
    Tree tree;
    constexpr int count = 1000;

    for (int i = 0; i < count; ++i) {
        double x = static_cast<double>(i);
        double y = static_cast<double>(count - i);
        tree.insert(i, Box(x, y, x + 0.5, y + 0.5));
    }

    if (tree.size() != static_cast<std::size_t>(count)) {
        return false;
    }

    auto results = tree.query(Box(-1, -1, count + 2, count + 2));
    if (results.size() != static_cast<std::size_t>(count)) {
        return false;
    }

    std::sort(results.begin(), results.end());
    for (int i = 0; i < count; ++i) {
        if (results[i] != i) {
            return false;
        }
    }
    return true;
}

bool test_spatial_filtering() {
    Tree tree;
    constexpr int grid = 20;
    int id = 0;

    for (int x = 0; x < grid; ++x) {
        for (int y = 0; y < grid; ++y) {
            tree.insert(id++, Box(x, y, x + 0.9, y + 0.9));
        }
    }

    auto subset = tree.query(Box(5.0, 5.0, 10.0, 10.0));

    std::unordered_set<int> expected;
    for (int x = 5; x <= 10; ++x) {
        for (int y = 5; y <= 10; ++y) {
            expected.insert(x * grid + y);
        }
    }

    for (int v : subset) {
        if (!expected.erase(v)) {
            std::cerr << "Unexpected item " << v << " in spatial query" << std::endl;
            return false;
        }
    }

    if (!expected.empty()) {
        std::cerr << "Missing " << expected.size() << " expected items in spatial query" << std::endl;
        return false;
    }

    auto outside = tree.query(Box(100.0, 100.0, 101.0, 101.0));
    if (!outside.empty()) {
        std::cerr << "Query outside populated area should be empty" << std::endl;
        return false;
    }

    return true;
}

bool test_overlapping_bounds() {
    Tree tree;
    tree.insert(1, Box(0, 0, 5, 5));
    tree.insert(2, Box(2, 2, 7, 7));
    tree.insert(3, Box(6, 6, 9, 9));

    auto center = tree.query(Box(3, 3, 4, 4));
    std::sort(center.begin(), center.end());
    if (center != std::vector<int>{1, 2}) {
        std::cerr << "Expected overlapping query to return items 1 and 2" << std::endl;
        return false;
    }

    auto edge = tree.query(Box(5, 5, 6, 6));
    std::sort(edge.begin(), edge.end());
    if (edge != std::vector<int>{1, 2, 3}) {
        std::cerr << "Expected edge query to return items 1, 2, and 3" << std::endl;
        return false;
    }

    return true;
}

bool run_all_tests() {
    const std::pair<const char*, bool (*)()> tests[] = {
        {"empty_tree", &test_empty_tree},
        {"single_insert", &test_single_insert},
        {"clear", &test_clear},
        {"bulk_insert_full_query", &test_bulk_insert_full_query},
        {"spatial_filtering", &test_spatial_filtering},
        {"overlapping_bounds", &test_overlapping_bounds},
    };

    bool all_passed = true;

    for (const auto& [name, fn] : tests) {
        if (!fn()) {
            std::cerr << "Test failed: " << name << std::endl;
            all_passed = false;
        }
    }

    return all_passed;
}

} // namespace rtree_tests

} // namespace gisevo

int main() {
    if (gisevo::rtree_tests::run_all_tests()) {
        std::cout << "All RTree tests passed" << std::endl;
        return 0;
    }

    std::cerr << "RTree tests failed" << std::endl;
    return 1;
}
