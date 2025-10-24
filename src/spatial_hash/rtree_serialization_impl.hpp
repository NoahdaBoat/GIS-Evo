#pragma once

// RTree serialization implementation
// This file contains template implementations for RTree serialization/deserialization
// It should be included at the end of rtree.hpp

namespace gisevo {

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
