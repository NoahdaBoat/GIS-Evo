#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../include/bounding_box.hpp"

namespace gisevo {

template <typename T>
class RTree {
public:
    struct Options {
        bool enable_spatial_clustering = false;
        bool enable_query_cache = false;
        bool enable_memory_pool = false;
        bool enable_space_filling_sort = false;
        std::size_t cache_capacity = 256;
        double cache_quantization = 1e-5;
    };

    explicit RTree(const Options& options = Options());
    ~RTree();

    void insert(const T& item, const BoundingBox& bounds);
    void bulk_load(std::vector<std::pair<T, BoundingBox>> entries);
    std::vector<T> query(const BoundingBox& bounds) const;
    std::vector<T> query(double min_x, double min_y, double max_x, double max_y) const;
    void clear();
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t depth() const;
    
    // Validation method to check tree structure
    bool validate_structure() const;

    // Serialization methods
    void serialize(std::ostream& out) const;
    void deserialize(std::istream& in);

    [[nodiscard]] const Options& options() const noexcept { return options_; }

private:
    struct Item;
    struct Node;
    struct NodeDeleter;
    using NodePtr = std::unique_ptr<Node, NodeDeleter>;

    class NodePool;
    class QueryCache;

    static constexpr std::size_t kMaxItems = 64;
    static constexpr std::size_t kMinItems = 16;
    static constexpr double kEpsilon = 1e-9;
    
    // Serialization version constants
    static constexpr std::uint32_t kSerializationVersion = 1;
    static constexpr const char* kSerializationMagic = "RTREE1";

    template <typename Container, typename Projection>
    static void sort_by_best_axis(Container& container, Projection projection);

    template <typename Container, typename Projection>
    void apply_spatial_sort(Container& container, Projection projection) const;

    [[nodiscard]] std::vector<std::size_t> calculate_group_sizes(std::size_t total) const;
    [[nodiscard]] std::vector<NodePtr> build_leaf_level(std::vector<Item>&& items);
    [[nodiscard]] std::vector<NodePtr> build_parent_level(std::vector<NodePtr>&& children);
    [[nodiscard]] NodePtr build_tree_from_items(std::vector<Item>&& items);
    [[nodiscard]] NodePtr split_node(Node* node);
    Node* choose_subtree(Node* node, const BoundingBox& bounds) const;
    NodePtr insert_recursive(Node* node, Item&& item, std::size_t depth = 0);
    void handle_root_split(NodePtr overflow);
    void query_recursive(const Node* node, const BoundingBox& bounds, std::vector<T>& results) const;
    void query_recursive_safe(const Node* node, const BoundingBox& bounds, std::vector<T>& results, std::size_t depth) const;
    void invalidate_cache() const;

    [[nodiscard]] double expansion_metric(const BoundingBox& target, const BoundingBox& candidate) const;
    [[nodiscard]] double clustering_metric(const BoundingBox& target, const BoundingBox& candidate) const;
    [[nodiscard]] static double perimeter_metric(const BoundingBox& bounds);
    [[nodiscard]] static double center_distance(const BoundingBox& a, const BoundingBox& b);
    [[nodiscard]] static std::uint64_t morton_code(double nx, double ny);
    [[nodiscard]] static std::uint64_t expand_bits(std::uint32_t value);

    NodePtr create_node(bool leaf);
    void release_node(Node* node);

    // Serialization helper methods
    void serialize_node(std::ostream& out, const Node* node) const;
    void deserialize_node(std::istream& in, Node* node);
    void deserialize_node_with_depth(std::istream& in, Node* node, std::size_t depth);
    void serialize_bounding_box(std::ostream& out, const BoundingBox& bounds) const;
    void deserialize_bounding_box(std::istream& in, BoundingBox& bounds);
    void serialize_item(std::ostream& out, const Item& item) const;
    void deserialize_item(std::istream& in, Item& item);
    void serialize_vector_size(std::ostream& out, std::size_t size) const;
    void deserialize_vector_size(std::istream& in, std::size_t& size);

    struct Item {
        T data;
        BoundingBox bounds;

        Item(const T& value, const BoundingBox& box)
            : data(value)
            , bounds(box) {}
    };

    struct Node {
        BoundingBox bounds;
        std::vector<Item> items;
        std::vector<NodePtr> children;
        bool is_leaf;

        explicit Node(bool leaf = true)
            : bounds()
            , items()
            , children()
            , is_leaf(leaf) {}

        void update_bounds() {
            if (is_leaf) {
                if (items.empty()) {
                    bounds = BoundingBox();
                    return;
                }

                bounds = BoundingBox(std::numeric_limits<double>::max(),
                                      std::numeric_limits<double>::max(),
                                      std::numeric_limits<double>::lowest(),
                                      std::numeric_limits<double>::lowest());

                for (const auto& item : items) {
                    bounds.min_x = std::min(bounds.min_x, item.bounds.min_x);
                    bounds.min_y = std::min(bounds.min_y, item.bounds.min_y);
                    bounds.max_x = std::max(bounds.max_x, item.bounds.max_x);
                    bounds.max_y = std::max(bounds.max_y, item.bounds.max_y);
                }
            } else {
                if (children.empty()) {
                    bounds = BoundingBox();
                    return;
                }

                bounds = children.front()->bounds;
                for (std::size_t i = 1; i < children.size(); ++i) {
                    bounds.expand(children[i]->bounds);
                }
            }
        }
    };

    struct NodeDeleter {
        RTree* owner{};

        void operator()(Node* node) const noexcept {
            if (owner) {
                owner->release_node(node);
            }
        }
    };

    class NodePool {
    public:
        explicit NodePool(bool enabled)
            : enabled_(enabled) {}

        Node* acquire(bool leaf) {
            if (enabled_) {
                if (!freelist_.empty()) {
                    Node* node = freelist_.back();
                    freelist_.pop_back();
                    node->bounds = BoundingBox();
                    node->items.clear();
                    node->children.clear();
                    node->is_leaf = leaf;
                    return node;
                }

                Node* node = new Node(leaf);
                arena_.push_back(node);
                return node;
            }

            return new Node(leaf);
        }

        void release(Node* node) {
            if (!node) {
                return;
            }

            node->bounds = BoundingBox();
            node->items.clear();
            node->children.clear();
            node->is_leaf = true;

            if (enabled_) {
                freelist_.push_back(node);
            } else {
                delete node;
            }
        }

        void clear() {
            if (!enabled_) {
                return;
            }

            freelist_.clear();
            for (Node* node : arena_) {
                delete node;
            }
            arena_.clear();
        }

        [[nodiscard]] bool enabled() const noexcept { return enabled_; }

        ~NodePool() {
            clear();
        }

    private:
        bool enabled_ = false;
        std::vector<Node*> arena_;
        std::vector<Node*> freelist_;
    };

    class QueryCache {
    public:
        QueryCache(bool enabled, std::size_t capacity, double quantization)
            : enabled_(enabled && capacity > 0)
            , capacity_(capacity)
            , quantization_(quantization) {}

        [[nodiscard]] bool enabled() const noexcept { return enabled_; }

        std::vector<T>* lookup(const BoundingBox& bounds) {
            if (!enabled_) {
                return nullptr;
            }

            const CacheKey key = make_key(bounds);
            const auto it = map_.find(key);
            if (it == map_.end()) {
                return nullptr;
            }

            entries_.splice(entries_.begin(), entries_, it->second);
            return &entries_.front().results;
        }

        void store(const BoundingBox& bounds, const std::vector<T>& results) {
            if (!enabled_) {
                return;
            }

            const CacheKey key = make_key(bounds);
            const auto it = map_.find(key);
            if (it != map_.end()) {
                it->second->results = results;
                entries_.splice(entries_.begin(), entries_, it->second);
                return;
            }

            if (map_.size() == capacity_) {
                auto tail = std::prev(entries_.end());
                map_.erase(tail->key);
                entries_.pop_back();
            }

            entries_.push_front(Entry{key, bounds, results});
            map_[key] = entries_.begin();
        }

        void clear() {
            entries_.clear();
            map_.clear();
        }

    private:
        struct CacheKey {
            std::int64_t min_x;
            std::int64_t min_y;
            std::int64_t max_x;
            std::int64_t max_y;

            bool operator==(const CacheKey& other) const {
                return min_x == other.min_x && min_y == other.min_y &&
                       max_x == other.max_x && max_y == other.max_y;
            }
        };

        struct Entry {
            CacheKey key;
            BoundingBox bounds;
            std::vector<T> results;
        };

        struct CacheKeyHasher {
            std::size_t operator()(const CacheKey& key) const noexcept {
                std::size_t hash = std::hash<std::int64_t>{}(key.min_x);
                hash ^= std::hash<std::int64_t>{}(key.min_y) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
                hash ^= std::hash<std::int64_t>{}(key.max_x) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
                hash ^= std::hash<std::int64_t>{}(key.max_y) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
                return hash;
            }
        };

        using CacheList = std::list<Entry>;
        using CacheMap = std::unordered_map<CacheKey, typename CacheList::iterator, CacheKeyHasher>;

        CacheKey make_key(const BoundingBox& bounds) const {
    return CacheKey{
        quantize(bounds.min_x),
        quantize(bounds.min_y),
        quantize(bounds.max_x),
        quantize(bounds.max_y)
    };
        }

        std::int64_t quantize(double value) const {
            return static_cast<std::int64_t>(std::llround(value / quantization_));
        }

        bool enabled_ = false;
        std::size_t capacity_ = 0;
        double quantization_ = 1e-5;
        CacheList entries_;
        CacheMap map_;
    };

    Options options_;
    std::unique_ptr<NodePool> pool_;
    mutable std::unique_ptr<QueryCache> cache_;
    NodePtr root_;
};

template <typename T>
RTree<T>::RTree(const Options& options)
    : options_(options)
    , pool_(std::make_unique<NodePool>(options.enable_memory_pool))
    , cache_(options.enable_query_cache
                  ? std::make_unique<QueryCache>(true, options.cache_capacity, options.cache_quantization)
                  : nullptr)
    , root_(create_node(true)) {}

template <typename T>
RTree<T>::~RTree() {
    root_.reset();
    if (pool_) {
        pool_->clear();
    }
}

template <typename T>
void RTree<T>::insert(const T& item, const BoundingBox& bounds) {
    invalidate_cache();
    NodePtr overflow = insert_recursive(root_.get(), Item(item, bounds));
    if (overflow) {
        handle_root_split(std::move(overflow));
    }
}

template <typename T>
void RTree<T>::bulk_load(std::vector<std::pair<T, BoundingBox>> entries) {
    invalidate_cache();
    root_.reset();

    if (entries.empty()) {
        root_ = create_node(true);
        return;
    }

    std::vector<Item> items;
    items.reserve(entries.size());
    for (auto& entry : entries) {
        items.emplace_back(entry.first, entry.second);
    }

    root_ = build_tree_from_items(std::move(items));
}

template <typename T>
std::vector<T> RTree<T>::query(const BoundingBox& bounds) const {
    if (cache_ && cache_->enabled()) {
        if (auto* cached = cache_->lookup(bounds)) {
            return *cached;
        }
    }

    std::vector<T> results;
    query_recursive(root_.get(), bounds, results);

    if (cache_ && cache_->enabled()) {
        cache_->store(bounds, results);
    }

    return results;
}

template <typename T>
std::vector<T> RTree<T>::query(double min_x, double min_y, double max_x, double max_y) const {
    return query(BoundingBox(min_x, min_y, max_x, max_y));
}

template <typename T>
void RTree<T>::clear() {
    invalidate_cache();
    root_.reset();
    root_ = create_node(true);
}

template <typename T>
std::size_t RTree<T>::size() const {
    std::size_t count = 0;

    std::vector<const Node*> stack;
    stack.push_back(root_.get());

    while (!stack.empty()) {
        const Node* node = stack.back();
        stack.pop_back();
        if (!node) {
            continue;
        }

        if (node->is_leaf) {
            count += node->items.size();
        } else {
            for (const auto& child : node->children) {
                stack.push_back(child.get());
            }
        }
    }

    return count;
}

template <typename T>
std::size_t RTree<T>::depth() const {
    if (!root_) {
        return 0;
    }

    std::size_t max_depth = 0;
    std::vector<std::pair<const Node*, std::size_t>> stack;
    stack.emplace_back(root_.get(), 1);

    while (!stack.empty()) {
        auto [node, current_depth] = stack.back();
        stack.pop_back();

        if (!node) {
            continue;
        }

        max_depth = std::max(max_depth, current_depth);

        if (!node->is_leaf) {
            for (const auto& child : node->children) {
                stack.emplace_back(child.get(), current_depth + 1);
            }
        }
    }

    return max_depth;
}

template <typename T>
bool RTree<T>::validate_structure() const {
    bool valid = true;
    std::vector<std::pair<const Node*, std::size_t>> stack;
    stack.emplace_back(root_.get(), 0);

    while (!stack.empty()) {
        auto [node, depth] = stack.back();
        stack.pop_back();

        if (!node) {
            continue;
        }

        if (node->is_leaf) {
            if (node->items.size() > kMaxItems) {
                std::cerr << "ERROR: Leaf node at depth " << depth 
                          << " has " << node->items.size() << " items (max: " << kMaxItems << ")" << std::endl;
                valid = false;
            }
        } else {
            if (node->children.size() > kMaxItems) {
                std::cerr << "ERROR: Non-leaf node at depth " << depth 
                          << " has " << node->children.size() << " children (max: " << kMaxItems << ")" << std::endl;
                valid = false;
            }
            for (const auto& child : node->children) {
                stack.emplace_back(child.get(), depth + 1);
            }
        }
    }

    if (!valid) {
        std::cerr << "R-tree structure validation FAILED!" << std::endl;
    }
    return valid;
}

template <typename T>
template <typename Container, typename Projection>
void RTree<T>::sort_by_best_axis(Container& container, Projection projection) {
    if (container.size() < 2) {
        return;
    }

    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();

    for (const auto& element : container) {
        const auto bounds = projection(element);
        const double center_x = (bounds.min_x + bounds.max_x) * 0.5;
        const double center_y = (bounds.min_y + bounds.max_y) * 0.5;
        min_x = std::min(min_x, center_x);
        max_x = std::max(max_x, center_x);
        min_y = std::min(min_y, center_y);
        max_y = std::max(max_y, center_y);
    }

    const double x_span = max_x - min_x;
    const double y_span = max_y - min_y;

    if (x_span >= y_span) {
        std::sort(container.begin(), container.end(), [&projection](const auto& lhs, const auto& rhs) {
            const auto lb = projection(lhs);
            const auto rb = projection(rhs);
            const double l = (lb.min_x + lb.max_x) * 0.5;
            const double r = (rb.min_x + rb.max_x) * 0.5;
            return l < r;
        });
    } else {
        std::sort(container.begin(), container.end(), [&projection](const auto& lhs, const auto& rhs) {
            const auto lb = projection(lhs);
            const auto rb = projection(rhs);
            const double l = (lb.min_y + lb.max_y) * 0.5;
            const double r = (rb.min_y + rb.max_y) * 0.5;
            return l < r;
        });
    }
}

template <typename T>
template <typename Container, typename Projection>
void RTree<T>::apply_spatial_sort(Container& container, Projection projection) const {
    if (!options_.enable_space_filling_sort || container.size() < 2) {
        return;
    }

    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();

    for (const auto& element : container) {
        const auto bounds = projection(element);
        const double center_x = (bounds.min_x + bounds.max_x) * 0.5;
        const double center_y = (bounds.min_y + bounds.max_y) * 0.5;
        min_x = std::min(min_x, center_x);
        max_x = std::max(max_x, center_x);
        min_y = std::min(min_y, center_y);
        max_y = std::max(max_y, center_y);
    }

    const double denom_x = std::max(max_x - min_x, kEpsilon);
    const double denom_y = std::max(max_y - min_y, kEpsilon);

    auto encode = [&](const auto& element) {
        const auto bounds = projection(element);
        const double cx = (bounds.min_x + bounds.max_x) * 0.5;
        const double cy = (bounds.min_y + bounds.max_y) * 0.5;
        const double nx = (cx - min_x) / denom_x;
        const double ny = (cy - min_y) / denom_y;
        return morton_code(nx, ny);
    };

    std::sort(container.begin(), container.end(), [&](const auto& lhs, const auto& rhs) {
        return encode(lhs) < encode(rhs);
    });
}

template <typename T>
std::vector<std::size_t> RTree<T>::calculate_group_sizes(std::size_t total) const {
    if (total == 0) {
        return {};
    }

    if (total <= kMaxItems) {
        return {total};
    }

    const std::size_t group_count = (total + kMaxItems - 1) / kMaxItems;
    std::vector<std::size_t> sizes(group_count, total / group_count);
    std::size_t remainder = total % group_count;
    for (std::size_t i = 0; i < remainder; ++i) {
        ++sizes[i];
    }

    if (group_count > 1 && sizes.back() < kMinItems) {
        for (std::size_t i = 0; i + 1 < group_count && sizes.back() < kMinItems; ++i) {
            const std::size_t available = sizes[i] > kMinItems ? sizes[i] - kMinItems : 0;
            const std::size_t transfer = std::min(available, kMinItems - sizes.back());
            if (transfer == 0) {
                continue;
            }
            sizes[i] -= transfer;
            sizes.back() += transfer;
        }
    }

    return sizes;
}

template <typename T>
std::vector<typename RTree<T>::NodePtr> RTree<T>::build_leaf_level(std::vector<Item>&& items) {
    std::vector<NodePtr> leaves;
    if (items.empty()) {
        return leaves;
    }

    apply_spatial_sort(items, [](const Item& item) { return item.bounds; });
    sort_by_best_axis(items, [](const Item& item) { return item.bounds; });

    const auto group_sizes = calculate_group_sizes(items.size());
    leaves.reserve(group_sizes.size());

    std::size_t offset = 0;
    for (const std::size_t group_size : group_sizes) {
        NodePtr node = create_node(true);
        node->items.reserve(group_size);
        for (std::size_t i = 0; i < group_size; ++i) {
            node->items.push_back(std::move(items[offset++]));
        }
        node->update_bounds();
        leaves.push_back(std::move(node));
    }

    return leaves;
}

template <typename T>
std::vector<typename RTree<T>::NodePtr> RTree<T>::build_parent_level(std::vector<NodePtr>&& children) {
    std::vector<NodePtr> parents;
    if (children.empty()) {
        return parents;
    }

    apply_spatial_sort(children, [](const NodePtr& node) { return node->bounds; });
    sort_by_best_axis(children, [](const NodePtr& node) { return node->bounds; });

    const auto group_sizes = calculate_group_sizes(children.size());
    parents.reserve(group_sizes.size());

    std::size_t offset = 0;
    for (const std::size_t group_size : group_sizes) {
        NodePtr parent = create_node(false);
        parent->children.reserve(group_size);
        for (std::size_t i = 0; i < group_size; ++i) {
            parent->children.push_back(std::move(children[offset++]));
        }
        parent->update_bounds();
        parents.push_back(std::move(parent));
    }

    return parents;
}

template <typename T>
typename RTree<T>::NodePtr RTree<T>::build_tree_from_items(std::vector<Item>&& items) {
    auto level = build_leaf_level(std::move(items));
    if (level.empty()) {
        return create_node(true);
    }

    // CRITICAL: Must continue building parent levels until exactly ONE root node remains.
    // Previous bug: stopped when level.size() <= kMaxItems, which could leave multiple 
    // top-level nodes, creating a malformed tree that exceeded max depth during queries.
    // Fixed: Continue until level.size() == 1 to ensure proper single-root tree structure.
    while (level.size() > 1) {
        level = build_parent_level(std::move(level));
    }

    return std::move(level.front());
}

template <typename T>
typename RTree<T>::NodePtr RTree<T>::split_node(Node* node) {
    if (node->is_leaf) {
        if (node->items.size() <= kMaxItems) {
            node->update_bounds();
            return nullptr;
        }

        apply_spatial_sort(node->items, [](const Item& item) { return item.bounds; });
        sort_by_best_axis(node->items, [](const Item& item) { return item.bounds; });

        const std::size_t mid = node->items.size() / 2;
        NodePtr new_node = create_node(true);
        new_node->items.assign(std::make_move_iterator(node->items.begin() + mid),
                               std::make_move_iterator(node->items.end()));
        node->items.erase(node->items.begin() + mid, node->items.end());

        node->update_bounds();
        new_node->update_bounds();
        return new_node;
    }

    if (node->children.size() <= kMaxItems) {
        node->update_bounds();
        return nullptr;
    }

    apply_spatial_sort(node->children, [](const NodePtr& child) { return child->bounds; });
    sort_by_best_axis(node->children, [](const NodePtr& child) { return child->bounds; });

    const std::size_t mid = node->children.size() / 2;
    NodePtr new_node = create_node(false);
    new_node->children.reserve(node->children.size() - mid);
    for (auto it = node->children.begin() + mid; it != node->children.end(); ++it) {
        new_node->children.push_back(std::move(*it));
    }
    node->children.erase(node->children.begin() + mid, node->children.end());

    node->update_bounds();
    new_node->update_bounds();
    return new_node;
}

template <typename T>
typename RTree<T>::Node* RTree<T>::choose_subtree(Node* node, const BoundingBox& bounds) const {
    if (node->is_leaf) {
        return node;
    }

    Node* best_child = nullptr;
    double best_expansion = std::numeric_limits<double>::max();
    double best_distance = std::numeric_limits<double>::max();
    double best_perimeter = std::numeric_limits<double>::max();

    for (auto& child_ptr : node->children) {
        Node* child = child_ptr.get();
        const double expansion = expansion_metric(bounds, child->bounds);
        const double distance = options_.enable_spatial_clustering ? clustering_metric(bounds, child->bounds) : 0.0;
        const double perimeter = perimeter_metric(child->bounds);

        const bool better_expansion = expansion < best_expansion - kEpsilon;
        const bool equal_expansion = std::abs(expansion - best_expansion) <= kEpsilon;

        bool better = false;
        if (!best_child || better_expansion) {
            better = true;
        } else if (equal_expansion) {
            if (options_.enable_spatial_clustering) {
                if (distance < best_distance - kEpsilon) {
                    better = true;
                } else if (std::abs(distance - best_distance) <= kEpsilon && perimeter < best_perimeter - kEpsilon) {
                    better = true;
                }
            } else if (perimeter < best_perimeter - kEpsilon) {
                better = true;
            }
        }

        if (better) {
            best_child = child;
            best_expansion = expansion;
            best_distance = distance;
            best_perimeter = perimeter;
        }
    }

    return best_child;
}

template <typename T>
typename RTree<T>::NodePtr RTree<T>::insert_recursive(Node* node, Item&& item, std::size_t depth) {
    // Prevent stack overflow from excessive recursion
    constexpr std::size_t kMaxDepth = 100;
    if (depth > kMaxDepth) {
        std::cerr << "Error: R-tree insert exceeded maximum depth (" << kMaxDepth 
                  << "). Tree structure may be corrupted." << std::endl;
        return nullptr;
    }

    if (node->is_leaf) {
        node->items.push_back(std::move(item));

        if (node->items.size() > kMaxItems) {
            return split_node(node);
        }

        node->update_bounds();
        return nullptr;
    }

    Node* best_child = choose_subtree(node, item.bounds);
    if (!best_child && !node->children.empty()) {
        best_child = node->children.front().get();
    }

    if (!best_child) {
        node->children.push_back(create_node(true));
        best_child = node->children.back().get();
    }

    NodePtr overflow = insert_recursive(best_child, std::move(item), depth + 1);
    if (overflow) {
        node->children.push_back(std::move(overflow));

        if (node->children.size() > kMaxItems) {
            return split_node(node);
        }
    }

    node->update_bounds();
    return nullptr;
}

template <typename T>
void RTree<T>::handle_root_split(NodePtr overflow) {
    NodePtr new_root = create_node(false);
    new_root->children.push_back(std::move(root_));
    new_root->children.push_back(std::move(overflow));
    new_root->update_bounds();
    root_ = std::move(new_root);
}

template <typename T>
void RTree<T>::query_recursive(const Node* node, const BoundingBox& bounds, std::vector<T>& results) const {
    query_recursive_safe(node, bounds, results, 0);
}

template <typename T>
void RTree<T>::query_recursive_safe(const Node* node, const BoundingBox& bounds, std::vector<T>& results, std::size_t depth) const {
    // Prevent stack overflow from corrupted data or circular references
    constexpr std::size_t kMaxDepth = 1000;
    if (depth > kMaxDepth) {
        std::cerr << "Warning: R-tree query exceeded maximum depth (" << kMaxDepth 
                  << "). Possible circular reference or corrupted tree data. "
                  << "Consider deleting cache files." << std::endl;
        std::cerr << "Query bounds: [" << bounds.min_x << ", " << bounds.min_y 
                  << ", " << bounds.max_x << ", " << bounds.max_y << "]" << std::endl;
        if (node) {
            std::cerr << "Node bounds: [" << node->bounds.min_x << ", " << node->bounds.min_y 
                      << ", " << node->bounds.max_x << ", " << node->bounds.max_y << "]" << std::endl;
            std::cerr << "Node is_leaf: " << node->is_leaf 
                      << ", items: " << node->items.size() 
                      << ", children: " << node->children.size() << std::endl;
        }
        return;
    }
    
    // Debug output when depth gets suspiciously high
    if (depth > 50 && depth % 100 == 0) {
        std::cerr << "DEBUG: Query at depth " << depth << std::endl;
        if (node) {
            std::cerr << "  Node is_leaf: " << node->is_leaf 
                      << ", items: " << node->items.size() 
                      << ", children: " << node->children.size() << std::endl;
        }
    }
    
    if (!node || !node->bounds.intersects(bounds)) {
        return;
    }

    if (node->is_leaf) {
        for (const auto& item : node->items) {
            if (item.bounds.intersects(bounds)) {
                results.push_back(item.data);
            }
        }
    } else {
        for (const auto& child : node->children) {
            query_recursive_safe(child.get(), bounds, results, depth + 1);
        }
    }
}

template <typename T>
void RTree<T>::invalidate_cache() const {
    if (cache_ && cache_->enabled()) {
        cache_->clear();
    }
}

template <typename T>
double RTree<T>::expansion_metric(const BoundingBox& target, const BoundingBox& candidate) const {
    BoundingBox expanded = candidate;
    expanded.expand(target);
    return expanded.area() - candidate.area();
}

template <typename T>
double RTree<T>::clustering_metric(const BoundingBox& target, const BoundingBox& candidate) const {
    return center_distance(target, candidate);
}

template <typename T>
double RTree<T>::perimeter_metric(const BoundingBox& bounds) {
    return bounds.perimeter();
}

template <typename T>
double RTree<T>::center_distance(const BoundingBox& a, const BoundingBox& b) {
    const double ax = (a.min_x + a.max_x) * 0.5;
    const double ay = (a.min_y + a.max_y) * 0.5;
    const double bx = (b.min_x + b.max_x) * 0.5;
    const double by = (b.min_y + b.max_y) * 0.5;
    const double dx = ax - bx;
    const double dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

template <typename T>
std::uint64_t RTree<T>::expand_bits(std::uint32_t value) {
    std::uint64_t v = value & 0x0000FFFFu;
    v = (v | (v << 16)) & 0x0000FFFF0000FFFFULL;
    v = (v | (v << 8)) & 0x00FF00FF00FF00FFULL;
    v = (v | (v << 4)) & 0x0F0F0F0F0F0F0F0FULL;
    v = (v | (v << 2)) & 0x3333333333333333ULL;
    v = (v | (v << 1)) & 0x5555555555555555ULL;
    return v;
}

template <typename T>
std::uint64_t RTree<T>::morton_code(double nx, double ny) {
    const auto clamp01 = [](double value) {
        return std::min(1.0, std::max(0.0, value));
    };

    const std::uint32_t x = static_cast<std::uint32_t>(clamp01(nx) * 65535.0);
    const std::uint32_t y = static_cast<std::uint32_t>(clamp01(ny) * 65535.0);
    return (expand_bits(x) << 1) | expand_bits(y);
}

template <typename T>
typename RTree<T>::NodePtr RTree<T>::create_node(bool leaf) {
    Node* raw = pool_->acquire(leaf);
    return NodePtr(raw, NodeDeleter{this});
}

template <typename T>
void RTree<T>::release_node(Node* node) {
    if (!node) {
        return;
    }
    pool_->release(node);
}

// Serialization method implementations
template <typename T>
void RTree<T>::serialize(std::ostream& out) const {
    // Write magic and version
    out.write(kSerializationMagic, 6);
    out.write(reinterpret_cast<const char*>(&kSerializationVersion), sizeof(kSerializationVersion));
    
    // Write options
    out.write(reinterpret_cast<const char*>(&options_.enable_spatial_clustering), sizeof(options_.enable_spatial_clustering));
    out.write(reinterpret_cast<const char*>(&options_.enable_query_cache), sizeof(options_.enable_query_cache));
    out.write(reinterpret_cast<const char*>(&options_.enable_memory_pool), sizeof(options_.enable_memory_pool));
    out.write(reinterpret_cast<const char*>(&options_.enable_space_filling_sort), sizeof(options_.enable_space_filling_sort));
    out.write(reinterpret_cast<const char*>(&options_.cache_capacity), sizeof(options_.cache_capacity));
    out.write(reinterpret_cast<const char*>(&options_.cache_quantization), sizeof(options_.cache_quantization));
    
    // Serialize the tree structure
    serialize_node(out, root_.get());
}

template <typename T>
void RTree<T>::deserialize(std::istream& in) {
    // Clear existing data
    clear();
    
    // Read magic and version
    char magic[6];
    in.read(magic, 6);
    if (std::string(magic, 6) != kSerializationMagic) {
        throw std::runtime_error("Invalid R-tree serialization magic");
    }
    
    std::uint32_t version;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != kSerializationVersion) {
        throw std::runtime_error("Unsupported R-tree serialization version");
    }
    
    // Read options
    in.read(reinterpret_cast<char*>(&options_.enable_spatial_clustering), sizeof(options_.enable_spatial_clustering));
    in.read(reinterpret_cast<char*>(&options_.enable_query_cache), sizeof(options_.enable_query_cache));
    in.read(reinterpret_cast<char*>(&options_.enable_memory_pool), sizeof(options_.enable_memory_pool));
    in.read(reinterpret_cast<char*>(&options_.enable_space_filling_sort), sizeof(options_.enable_space_filling_sort));
    in.read(reinterpret_cast<char*>(&options_.cache_capacity), sizeof(options_.cache_capacity));
    in.read(reinterpret_cast<char*>(&options_.cache_quantization), sizeof(options_.cache_quantization));
    
    if (!in.good()) {
        throw std::runtime_error("Failed to read R-tree options during deserialization. Cache file may be corrupted.");
    }
    
    // CRITICAL: Reset root before recreating pool to avoid use-after-free
    // The old pool will delete all nodes, including root's node
    root_.reset();
    
    // Recreate pool and cache with new options
    pool_ = std::make_unique<NodePool>(options_.enable_memory_pool);
    cache_ = options_.enable_query_cache
        ? std::make_unique<QueryCache>(true, options_.cache_capacity, options_.cache_quantization)
        : nullptr;
    
    // Deserialize the tree structure with validation
    root_ = create_node(true); // Will be updated by deserialize_node
    try {
        deserialize_node(in, root_.get());
        if (!in.good() && !in.eof()) {
            throw std::runtime_error("Stream error after R-tree deserialization");
        }
    } catch (const std::exception& ex) {
        // Re-throw with more context
        throw std::runtime_error("R-tree deserialization failed: " + std::string(ex.what()));
    }
}

template <typename T>
void RTree<T>::serialize_node(std::ostream& out, const Node* node) const {
    if (!node) {
        // Write null marker
        const bool is_null = true;
        out.write(reinterpret_cast<const char*>(&is_null), sizeof(is_null));
        return;
    }
    
    // Write null marker (false)
    const bool is_null = false;
    out.write(reinterpret_cast<const char*>(&is_null), sizeof(is_null));
    
    // Write node properties
    out.write(reinterpret_cast<const char*>(&node->is_leaf), sizeof(node->is_leaf));
    serialize_bounding_box(out, node->bounds);
    
    if (node->is_leaf) {
        // Serialize items
        serialize_vector_size(out, node->items.size());
        for (const auto& item : node->items) {
            serialize_item(out, item);
        }
    } else {
        // Serialize children
        serialize_vector_size(out, node->children.size());
        for (const auto& child : node->children) {
            serialize_node(out, child.get());
        }
    }
}

template <typename T>
void RTree<T>::deserialize_node(std::istream& in, Node* node) {
    deserialize_node_with_depth(in, node, 0);
}

template <typename T>
void RTree<T>::deserialize_node_with_depth(std::istream& in, Node* node, std::size_t depth) {
    // Validate depth to prevent stack overflow and detect circular references
    constexpr std::size_t kMaxDeserializationDepth = 100;
    if (depth > kMaxDeserializationDepth) {
        throw std::runtime_error("R-tree deserialization exceeded maximum depth (" + 
                                std::to_string(kMaxDeserializationDepth) + 
                                "). Cache file is corrupted with circular references or excessive depth.");
    }
    
    // Read null marker
    bool is_null;
    in.read(reinterpret_cast<char*>(&is_null), sizeof(is_null));
    if (!in.good()) {
        throw std::runtime_error("Failed to read null marker during R-tree deserialization at depth " + 
                                std::to_string(depth));
    }
    if (is_null) {
        return;
    }
    
    // Read node properties
    in.read(reinterpret_cast<char*>(&node->is_leaf), sizeof(node->is_leaf));
    if (!in.good()) {
        throw std::runtime_error("Failed to read node type during R-tree deserialization at depth " + 
                                std::to_string(depth));
    }
    
    deserialize_bounding_box(in, node->bounds);
    if (!in.good()) {
        throw std::runtime_error("Failed to read bounding box during R-tree deserialization at depth " + 
                                std::to_string(depth));
    }
    
    // Validate bounding box
    if (!std::isfinite(node->bounds.min_x) || !std::isfinite(node->bounds.min_y) ||
        !std::isfinite(node->bounds.max_x) || !std::isfinite(node->bounds.max_y)) {
        throw std::runtime_error("Invalid bounding box with non-finite values in R-tree at depth " + 
                                std::to_string(depth) + ". Cache file is corrupted.");
    }
    
    if (node->is_leaf) {
        // Deserialize items
        std::size_t item_count;
        deserialize_vector_size(in, item_count);
        if (!in.good()) {
            throw std::runtime_error("Failed to read item count during R-tree deserialization at depth " + 
                                    std::to_string(depth));
        }
        
        // Validate item count is reasonable
        constexpr std::size_t kMaxReasonableItems = 1000000;
        if (item_count > kMaxReasonableItems) {
            throw std::runtime_error("Unreasonable item count (" + std::to_string(item_count) + 
                                    ") in R-tree leaf node at depth " + std::to_string(depth) + 
                                    ". Cache file is corrupted.");
        }
        
        node->items.reserve(item_count);
        
        for (std::size_t i = 0; i < item_count; ++i) {
            Item item(T{}, BoundingBox{});
            deserialize_item(in, item);
            if (!in.good()) {
                throw std::runtime_error("Failed to read item " + std::to_string(i) + 
                                        " during R-tree deserialization at depth " + std::to_string(depth));
            }
            node->items.push_back(std::move(item));
        }
    } else {
        // Deserialize children
        std::size_t child_count;
        deserialize_vector_size(in, child_count);
        if (!in.good()) {
            throw std::runtime_error("Failed to read child count during R-tree deserialization at depth " + 
                                    std::to_string(depth));
        }
        
        // Validate child count is reasonable
        constexpr std::size_t kMaxReasonableChildren = 1000;
        if (child_count > kMaxReasonableChildren) {
            throw std::runtime_error("Unreasonable child count (" + std::to_string(child_count) + 
                                    ") in R-tree node at depth " + std::to_string(depth) + 
                                    ". Cache file is corrupted.");
        }
        
        node->children.reserve(child_count);
        
        for (std::size_t i = 0; i < child_count; ++i) {
            NodePtr child = create_node(true); // Will be updated by deserialize_node_with_depth
            deserialize_node_with_depth(in, child.get(), depth + 1);
            node->children.push_back(std::move(child));
        }
    }
}

template <typename T>
void RTree<T>::serialize_bounding_box(std::ostream& out, const BoundingBox& bounds) const {
    out.write(reinterpret_cast<const char*>(&bounds.min_x), sizeof(bounds.min_x));
    out.write(reinterpret_cast<const char*>(&bounds.min_y), sizeof(bounds.min_y));
    out.write(reinterpret_cast<const char*>(&bounds.max_x), sizeof(bounds.max_x));
    out.write(reinterpret_cast<const char*>(&bounds.max_y), sizeof(bounds.max_y));
}

template <typename T>
void RTree<T>::deserialize_bounding_box(std::istream& in, BoundingBox& bounds) {
    in.read(reinterpret_cast<char*>(&bounds.min_x), sizeof(bounds.min_x));
    in.read(reinterpret_cast<char*>(&bounds.min_y), sizeof(bounds.min_y));
    in.read(reinterpret_cast<char*>(&bounds.max_x), sizeof(bounds.max_x));
    in.read(reinterpret_cast<char*>(&bounds.max_y), sizeof(bounds.max_y));
}

template <typename T>
void RTree<T>::serialize_item(std::ostream& out, const Item& item) const {
    // Serialize the data (T type)
    out.write(reinterpret_cast<const char*>(&item.data), sizeof(item.data));
    serialize_bounding_box(out, item.bounds);
}

template <typename T>
void RTree<T>::deserialize_item(std::istream& in, Item& item) {
    // Deserialize the data (T type)
    in.read(reinterpret_cast<char*>(&item.data), sizeof(item.data));
    deserialize_bounding_box(in, item.bounds);
}

template <typename T>
void RTree<T>::serialize_vector_size(std::ostream& out, std::size_t size) const {
    const std::uint64_t size_64 = static_cast<std::uint64_t>(size);
    out.write(reinterpret_cast<const char*>(&size_64), sizeof(size_64));
}

template <typename T>
void RTree<T>::deserialize_vector_size(std::istream& in, std::size_t& size) {
    std::uint64_t size_64;
    in.read(reinterpret_cast<char*>(&size_64), sizeof(size_64));
    size = static_cast<std::size_t>(size_64);
}

} // namespace gisevo
