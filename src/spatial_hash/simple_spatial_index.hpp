#pragma once

#include <algorithm>
#include <vector>

#include "../include/bounding_box.hpp"

namespace gisevo {

// Simple spatial index using a vector of items with bounding boxes
template <typename T>
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

} // namespace gisevo
