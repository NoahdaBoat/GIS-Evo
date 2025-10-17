#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <iterator>
#include <limits>
#include <cmath>
#include <functional>
#include <iostream>

namespace gisevo {

// Simple bounding box structure
struct BoundingBox {
    double min_x, min_y, max_x, max_y;
    
    BoundingBox() : min_x(0), min_y(0), max_x(0), max_y(0) {}
    BoundingBox(double min_x, double min_y, double max_x, double max_y) 
        : min_x(min_x), min_y(min_y), max_x(max_x), max_y(max_y) {}
    
    // Check if this bounding box intersects with another
    bool intersects(const BoundingBox& other) const {
        return !(max_x < other.min_x || min_x > other.max_x ||
                 max_y < other.min_y || min_y > other.max_y);
    }
    
    // Check if this bounding box contains a point
    bool contains(double x, double y) const {
        return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
    }
    
    // Expand this bounding box to include another
    void expand(const BoundingBox& other) {
        min_x = std::min(min_x, other.min_x);
        min_y = std::min(min_y, other.min_y);
        max_x = std::max(max_x, other.max_x);
        max_y = std::max(max_y, other.max_y);
    }
    
    // Calculate area
    double area() const {
        return (max_x - min_x) * (max_y - min_y);
    }
    
    // Calculate center
    struct Point2D {
        double x, y;
        Point2D(double x = 0, double y = 0) : x(x), y(y) {}
    };
    
    Point2D center() const {
        return Point2D{(min_x + max_x) / 2.0, (min_y + max_y) / 2.0};
    }
};

// R-tree node structure
template<typename T>
struct RTreeNode {
    BoundingBox bounds;
    std::vector<T> items;  // For leaf nodes
    std::vector<std::unique_ptr<RTreeNode<T>>> children;  // For internal nodes
    bool is_leaf;
    
    RTreeNode(bool leaf = true) : is_leaf(leaf) {}
    
    // Calculate bounding box from children or items
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
            
            bounds = children[0]->bounds;
            for (size_t i = 1; i < children.size(); ++i) {
                bounds.expand(children[i]->bounds);
            }
        }
    }
};

// R-tree item wrapper
template<typename T>
struct RTreeItem {
    T data;
    BoundingBox bounds;
    
    RTreeItem(const T& item_data, const BoundingBox& item_bounds) 
        : data(item_data), bounds(item_bounds) {}
};

// Simple R-tree implementation
template<typename T>
class RTree {
private:
    static constexpr size_t MAX_ITEMS = 16;  // Maximum items per leaf node
    static constexpr size_t MIN_ITEMS = 4;   // Minimum items per leaf node
    
    std::unique_ptr<RTreeNode<RTreeItem<T>>> root_;
    
    // Split a node when it becomes too full
    std::unique_ptr<RTreeNode<RTreeItem<T>>> split_node(RTreeNode<RTreeItem<T>>* node) {
        if (node->is_leaf) {
            if (node->items.size() <= MAX_ITEMS) {
                node->update_bounds();
                return nullptr;
            }

            std::sort(node->items.begin(), node->items.end(), [](const auto& lhs, const auto& rhs) {
                const double lhs_center = (lhs.bounds.min_x + lhs.bounds.max_x) * 0.5;
                const double rhs_center = (rhs.bounds.min_x + rhs.bounds.max_x) * 0.5;
                return lhs_center < rhs_center;
            });

            const size_t mid = node->items.size() / 2;
            auto new_node = std::make_unique<RTreeNode<RTreeItem<T>>>(true);
            new_node->items.assign(std::make_move_iterator(node->items.begin() + mid),
                                   std::make_move_iterator(node->items.end()));
            node->items.erase(node->items.begin() + mid, node->items.end());

            node->update_bounds();
            new_node->update_bounds();
            return new_node;
        }

        if (node->children.size() <= MAX_ITEMS) {
            node->update_bounds();
            return nullptr;
        }

        std::sort(node->children.begin(), node->children.end(), [](const auto& lhs, const auto& rhs) {
            const double lhs_center = (lhs->bounds.min_x + lhs->bounds.max_x) * 0.5;
            const double rhs_center = (rhs->bounds.min_x + rhs->bounds.max_x) * 0.5;
            return lhs_center < rhs_center;
        });

        const size_t mid = node->children.size() / 2;
        auto new_node = std::make_unique<RTreeNode<RTreeItem<T>>>(false);
        for (size_t i = mid; i < node->children.size(); ++i) {
            new_node->children.push_back(std::move(node->children[i]));
        }
        node->children.erase(node->children.begin() + mid, node->children.end());

        node->update_bounds();
        new_node->update_bounds();
        return new_node;
    }
    
    // Choose the best subtree for insertion
    RTreeNode<RTreeItem<T>>* choose_subtree(RTreeNode<RTreeItem<T>>* node, const BoundingBox& bounds) {
        if (node->is_leaf) {
            return node;
        }
        
        double min_expansion = std::numeric_limits<double>::max();
        RTreeNode<RTreeItem<T>>* best_child = nullptr;
        
        for (auto& child : node->children) {
            BoundingBox expanded = child->bounds;
            expanded.expand(bounds);
            const double expansion = expanded.area() - child->bounds.area();
            
            if (expansion < min_expansion) {
                min_expansion = expansion;
                best_child = child.get();
            } else if (std::abs(expansion - min_expansion) < 1e-9) {
                if (child->bounds.area() < best_child->bounds.area()) {
                    best_child = child.get();
                }
            }
        }
        
        return best_child;
    }
    
    // Insert item into tree
    std::unique_ptr<RTreeNode<RTreeItem<T>>> insert_recursive(RTreeNode<RTreeItem<T>>* node,
                                                             RTreeItem<T>&& item) {
        if (node->is_leaf) {
            node->items.push_back(std::move(item));

            if (node->items.size() > MAX_ITEMS) {
                return split_node(node);
            }

            node->update_bounds();
            return nullptr;
        }

        RTreeNode<RTreeItem<T>>* best_child = choose_subtree(node, item.bounds);
        if (!best_child && !node->children.empty()) {
            best_child = node->children.front().get();
        }

        if (!best_child) {
            node->children.push_back(std::make_unique<RTreeNode<RTreeItem<T>>>(true));
            best_child = node->children.back().get();
        }

        auto overflow = insert_recursive(best_child, std::move(item));
        if (overflow) {
            node->children.push_back(std::move(overflow));

            if (node->children.size() > MAX_ITEMS) {
                return split_node(node);
            }
        }

        node->update_bounds();
        return nullptr;
    }

    void handle_root_split(std::unique_ptr<RTreeNode<RTreeItem<T>>> overflow) {
        auto new_root = std::make_unique<RTreeNode<RTreeItem<T>>>(false);
        new_root->children.push_back(std::move(root_));
        new_root->children.push_back(std::move(overflow));
        new_root->update_bounds();
        root_ = std::move(new_root);
    }
    
    // Query items within bounds
    void query_recursive(const RTreeNode<RTreeItem<T>>* node, const BoundingBox& bounds, 
                        std::vector<T>& results) const {
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
                query_recursive(child.get(), bounds, results);
            }
        }
    }
    
public:
    RTree() : root_(std::make_unique<RTreeNode<RTreeItem<T>>>(true)) {}
    
    // Insert an item with its bounding box
    void insert(const T& item, const BoundingBox& bounds) {
        auto overflow = insert_recursive(root_.get(), RTreeItem<T>(item, bounds));
        if (overflow) {
            handle_root_split(std::move(overflow));
        }
    }
    
    // Query all items within the given bounds
    std::vector<T> query(const BoundingBox& bounds) const {
        std::vector<T> results;
        query_recursive(root_.get(), bounds, results);
        return results;
    }
    
    // Query all items within a rectangular region
    std::vector<T> query(double min_x, double min_y, double max_x, double max_y) const {
        return query(BoundingBox(min_x, min_y, max_x, max_y));
    }
    
    // Clear all items
    void clear() {
        root_ = std::make_unique<RTreeNode<RTreeItem<T>>>(true);
    }
    
    // Get total number of items (approximate)
    size_t size() const {
        size_t count = 0;
        
        std::function<void(const RTreeNode<RTreeItem<T>>*)> count_recursive;
        count_recursive = [&count, &count_recursive](const RTreeNode<RTreeItem<T>>* node) {
            if (node->is_leaf) {
                count += node->items.size();
            } else {
                for (const auto& child : node->children) {
                    count_recursive(child.get());
                }
            }
        };
        
        count_recursive(root_.get());
        return count;
    }
};

} // namespace gisevo
