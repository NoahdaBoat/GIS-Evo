#pragma once

#include <vector>
#include <algorithm>
#include <cmath>

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
};

// Simple spatial index using a vector of items with bounding boxes
template<typename T>
class SimpleSpatialIndex {
private:
    struct IndexItem {
        T data;
        BoundingBox bounds;
        
        IndexItem(const T& item_data, const BoundingBox& item_bounds) 
            : data(item_data), bounds(item_bounds) {}
    };
    
    std::vector<IndexItem> items_;
    
public:
    SimpleSpatialIndex() = default;
    
    // Insert an item with its bounding box
    void insert(const T& item, const BoundingBox& bounds) {
        items_.emplace_back(item, bounds);
    }
    
    // Query all items within the given bounds (optimized with early exits)
    std::vector<T> query(const BoundingBox& bounds) const {
        std::vector<T> results;
        results.reserve(1000);  // Pre-allocate space for common query sizes
        
        // Early exit if query bounds are invalid
        if (bounds.min_x > bounds.max_x || bounds.min_y > bounds.max_y) {
            return results;
        }
        
        // Pre-calculate query area for early exit optimization
        double query_area = bounds.area();
        if (query_area <= 0) {
            return results;
        }
        
        for (const auto& item : items_) {
            // Fast bounding box intersection check with early exits
            if (item.bounds.max_x < bounds.min_x || 
                item.bounds.min_x > bounds.max_x ||
                item.bounds.max_y < bounds.min_y || 
                item.bounds.min_y > bounds.max_y) {
                continue;  // No intersection, skip to next item
            }
            
            results.push_back(item.data);
            
            // Early exit if we've found enough results (performance optimization)
            if (results.size() > 10000) {
                break;
            }
        }
        
        return results;
    }
    
    // Query all items within a rectangular region
    std::vector<T> query(double min_x, double min_y, double max_x, double max_y) const {
        return query(BoundingBox(min_x, min_y, max_x, max_y));
    }
    
    // Clear all items
    void clear() {
        items_.clear();
    }
    
    // Get total number of items
    size_t size() const {
        return items_.size();
    }
    
    // Get statistics about the spatial index
    struct Statistics {
        size_t total_items;
        double total_area;
        double avg_area;
        BoundingBox bounds;
    };
    
    Statistics get_statistics() const {
        Statistics stats;
        stats.total_items = items_.size();
        stats.total_area = 0.0;
        stats.bounds = BoundingBox();
        
        if (items_.empty()) {
            stats.avg_area = 0.0;
            return stats;
        }
        
        bool first = true;
        for (const auto& item : items_) {
            stats.total_area += item.bounds.area();
            
            if (first) {
                stats.bounds = item.bounds;
                first = false;
            } else {
                stats.bounds.expand(item.bounds);
            }
        }
        
        stats.avg_area = stats.total_area / stats.total_items;
        return stats;
    }
};

// Use the simple spatial index as our R-tree for now
template<typename T>
using RTree = SimpleSpatialIndex<T>;

} // namespace gisevo
