#include "binary_database.hpp"
#include "cache_manager.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <filesystem>

namespace gisevo {

BinaryDatabase& BinaryDatabase::instance() {
    static BinaryDatabase instance;
    return instance;
}

namespace {

RTree<std::size_t>::Options make_spatial_options() {
    RTree<std::size_t>::Options opts;
    opts.enable_spatial_clustering = true;
    opts.enable_query_cache = true;
    opts.enable_memory_pool = true;
    opts.enable_space_filling_sort = true;
    opts.cache_capacity = 1024;
    opts.cache_quantization = 1e-5;
    return opts;
}

} // namespace

BinaryDatabase::BinaryDatabase()
    : street_rtree_(make_spatial_options())
    , intersection_rtree_(make_spatial_options())
    , poi_rtree_(make_spatial_options())
    , feature_rtree_(make_spatial_options()) {}

template <typename T>
T read_pod(std::ifstream& in) {
    T value;
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in) {
        throw std::runtime_error("Failed to read POD type from binary file");
    }
    return value;
}

// Memory-mapped file helper functions
bool BinaryDatabase::map_file(const std::string& path, std::unique_ptr<MappedFile>& mmap) {
    mmap = std::make_unique<MappedFile>();
    
    // Open file
    mmap->fd = open(path.c_str(), O_RDONLY);
    if (mmap->fd == -1) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return false;
    }
    
    // Get file size
    struct stat sb;
    if (fstat(mmap->fd, &sb) == -1) {
        std::cerr << "Failed to get file size: " << path << std::endl;
        return false;
    }
    mmap->size = sb.st_size;
    
    // Memory map the file
    mmap->data = ::mmap(nullptr, mmap->size, PROT_READ, MAP_PRIVATE, mmap->fd, 0);
    if (mmap->data == MAP_FAILED) {
        std::cerr << "Failed to memory map file: " << path << std::endl;
        return false;
    }
    
    return true;
}

template <typename T>
T read_pod_mmap(const char*& ptr) {
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

std::string read_string_mmap(const char*& ptr) {
    const uint32_t length = read_pod_mmap<uint32_t>(ptr);
    std::string result(ptr, length);
    ptr += length;
    return result;
}

std::vector<OSMID> read_node_refs_mmap(const char*& ptr) {
    const uint32_t count = read_pod_mmap<uint32_t>(ptr);
    std::vector<OSMID> refs(count);
    if (count > 0) {
        std::memcpy(refs.data(), ptr, count * sizeof(OSMID));
        ptr += count * sizeof(OSMID);
    }
    return refs;
}

std::vector<std::pair<std::string, std::string>> read_tags_mmap(const char*& ptr) {
    const uint32_t tag_count = read_pod_mmap<uint32_t>(ptr);
    std::vector<std::pair<std::string, std::string>> tags;
    tags.reserve(tag_count);
    
    for (uint32_t i = 0; i < tag_count; ++i) {
        std::string key = read_string_mmap(ptr);
        std::string value = read_string_mmap(ptr);
        tags.emplace_back(std::move(key), std::move(value));
    }
    
    return tags;
}

std::string read_string(std::ifstream& in) {
    const uint32_t length = read_pod<uint32_t>(in);
    std::string result(length, '\0');
    in.read(result.data(), length);
    if (!in) {
        throw std::runtime_error("Failed to read string from binary file");
    }
    return result;
}

std::vector<OSMID> read_node_refs(std::ifstream& in) {
    const uint32_t count = read_pod<uint32_t>(in);
    std::vector<OSMID> refs(count);
    if (count > 0) {
        in.read(reinterpret_cast<char*>(refs.data()), count * sizeof(OSMID));
        if (!in) {
            throw std::runtime_error("Failed to read node refs from binary file");
        }
    }
    return refs;
}

std::vector<std::pair<std::string, std::string>> read_tags(std::ifstream& in) {
    const uint32_t tag_count = read_pod<uint32_t>(in);
    std::vector<std::pair<std::string, std::string>> tags;
    tags.reserve(tag_count);
    
    for (uint32_t i = 0; i < tag_count; ++i) {
        std::string key = read_string(in);
        std::string value = read_string(in);
        tags.emplace_back(std::move(key), std::move(value));
    }
    
    return tags;
}

bool BinaryDatabase::load_streets_file(const std::string& path) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Try memory-mapped loading first
    if (!map_file(path, streets_mmap_)) {
        std::cerr << "Memory mapping failed, falling back to stream loading" << std::endl;
        return load_streets_file_stream(path);
    }
    
    try {
        const char* ptr = static_cast<const char*>(streets_mmap_->data);
        
        // Read and verify magic
        if (std::memcmp(ptr, "GISEVOS1", 8) != 0 && std::memcmp(ptr, "GISEVOS2", 8) != 0) {
            throw std::runtime_error("Invalid streets.bin magic header");
        }
        ptr += 8;
        
        // Read version
        const uint32_t version = read_pod_mmap<uint32_t>(ptr);
        if (version != 1 && version != 2) {
            throw std::runtime_error("Unsupported streets.bin version");
        }
        
        // Read counts
        const uint64_t node_count = read_pod_mmap<uint64_t>(ptr);
        const uint64_t segment_count = read_pod_mmap<uint64_t>(ptr);
        
        std::cout << "Loading " << node_count << " nodes and " << segment_count << " segments..." << std::endl;
        
        // Reserve space for bulk loading
        nodes_.reserve(node_count);
        segments_.reserve(segment_count);
        
        // Load nodes (version-dependent)
        if (version == 1) {
            // Version 1: bulk load nodes without tags
            const Node* nodes_ptr = reinterpret_cast<const Node*>(ptr);
            nodes_.assign(nodes_ptr, nodes_ptr + node_count);
            ptr += node_count * (sizeof(OSMID) + sizeof(double) + sizeof(double));
        } else {
            // Version 2: load nodes with tags individually
            for (uint64_t i = 0; i < node_count; ++i) {
                Node node;
                node.osm_id = read_pod_mmap<int64_t>(ptr);
                node.lat = read_pod_mmap<double>(ptr);
                node.lon = read_pod_mmap<double>(ptr);
                node.tags = read_tags_mmap(ptr);
                nodes_.push_back(node);
            }
        }
        
        // Load segments (these have variable-length strings, so we need to parse individually)
        for (uint64_t i = 0; i < segment_count; ++i) {
            StreetSegment seg;
            seg.osm_id = read_pod_mmap<int64_t>(ptr);
            seg.category = read_pod_mmap<uint8_t>(ptr);
            seg.max_speed_kph = read_pod_mmap<float>(ptr);
            seg.name = read_string_mmap(ptr);
            seg.node_refs = read_node_refs_mmap(ptr);
            // Check if the way is closed (first and last nodes are the same)
            seg.is_closed = (seg.node_refs.size() >= 2 && 
                            seg.node_refs.front() == seg.node_refs.back());
            if (version == 2) {
                seg.tags = read_tags_mmap(ptr);
            }
            segments_.push_back(seg);
        }
        
        build_indexes();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Memory-mapped loading completed in " << duration.count() << "ms" << std::endl;
        
        return true;
        
    } catch (const std::exception& ex) {
        std::cerr << "Error loading streets file: " << ex.what() << std::endl;
        clear();
        return false;
    }
}

// Fallback stream-based loading for compatibility
bool BinaryDatabase::load_streets_file_stream(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    
    try {
        // Read and verify magic
        char magic[8];
        in.read(magic, 8);
        if (std::memcmp(magic, "GISEVOS1", 8) != 0 && std::memcmp(magic, "GISEVOS2", 8) != 0) {
            throw std::runtime_error("Invalid streets.bin magic header");
        }
        
        // Read version
        const uint32_t version = read_pod<uint32_t>(in);
        if (version != 1 && version != 2) {
            throw std::runtime_error("Unsupported streets.bin version");
        }
        
        // Read counts
        const uint64_t node_count = read_pod<uint64_t>(in);
        const uint64_t segment_count = read_pod<uint64_t>(in);
        
        // Read nodes
        nodes_.reserve(node_count);
        for (uint64_t i = 0; i < node_count; ++i) {
            Node node;
            node.osm_id = read_pod<int64_t>(in);
            node.lat = read_pod<double>(in);
            node.lon = read_pod<double>(in);
            if (version == 2) {
                node.tags = read_tags(in);
            }
            nodes_.push_back(node);
        }
        
        // Read segments
        segments_.reserve(segment_count);
        for (uint64_t i = 0; i < segment_count; ++i) {
            StreetSegment seg;
            seg.osm_id = read_pod<int64_t>(in);
            seg.category = read_pod<uint8_t>(in);
            seg.max_speed_kph = read_pod<float>(in);
            seg.name = read_string(in);
            seg.node_refs = read_node_refs(in);
            // Check if the way is closed (first and last nodes are the same)
            seg.is_closed = (seg.node_refs.size() >= 2 && 
                            seg.node_refs.front() == seg.node_refs.back());
            if (version == 2) {
                seg.tags = read_tags(in);
            }
            segments_.push_back(seg);
        }
        
        build_indexes();
        return true;
        
    } catch (const std::exception& ex) {
        clear();
        return false;
    }
}

bool BinaryDatabase::load_osm_file(const std::string& path) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Try memory-mapped loading first
    if (!map_file(path, osm_mmap_)) {
        std::cerr << "Memory mapping failed for OSM file, falling back to stream loading" << std::endl;
        return load_osm_file_stream(path);
    }
    
    try {
        const char* ptr = static_cast<const char*>(osm_mmap_->data);
        
        // Read and verify magic
        if (std::memcmp(ptr, "GISEVOO1", 8) != 0 && std::memcmp(ptr, "GISEVOO2", 8) != 0) {
            throw std::runtime_error("Invalid osm.bin magic header");
        }
        ptr += 8;
        
        // Read version
        const uint32_t version = read_pod_mmap<uint32_t>(ptr);
        if (version != 1 && version != 2) {
            throw std::runtime_error("Unsupported osm.bin version");
        }
        
        // Read POI count
        const uint64_t poi_count = read_pod_mmap<uint64_t>(ptr);
        
        std::cout << "Loading " << poi_count << " POIs..." << std::endl;
        
        // Read POIs
        pois_.reserve(poi_count);
        for (uint64_t i = 0; i < poi_count; ++i) {
            POI poi;
            poi.osm_id = read_pod_mmap<int64_t>(ptr);
            poi.lat = read_pod_mmap<double>(ptr);
            poi.lon = read_pod_mmap<double>(ptr);
            poi.category = read_string_mmap(ptr);
            poi.name = read_string_mmap(ptr);
            if (version == 2) {
                poi.tags = read_tags_mmap(ptr);
            }
            pois_.push_back(poi);
        }
        
        // Read feature count
        const uint64_t feature_count = read_pod_mmap<uint64_t>(ptr);
        
        std::cout << "Loading " << feature_count << " features..." << std::endl;
        
        // Read features
        features_.reserve(feature_count);
        for (uint64_t i = 0; i < feature_count; ++i) {
            Feature feature;
            feature.osm_id = read_pod_mmap<int64_t>(ptr);
            feature.type = static_cast<FeatureType>(read_pod_mmap<uint8_t>(ptr));
            feature.name = read_string_mmap(ptr);
            
            const uint64_t node_ref_count = read_pod_mmap<uint64_t>(ptr);
            feature.node_refs.reserve(node_ref_count);
            for (uint64_t j = 0; j < node_ref_count; ++j) {
                feature.node_refs.push_back(read_pod_mmap<int64_t>(ptr));
            }
            
            feature.is_closed = read_pod_mmap<bool>(ptr);
            if (version == 2) {
                feature.tags = read_tags_mmap(ptr);
            }
            features_.push_back(feature);
        }
        
        // Read relations (version 2 only)
        if (version == 2) {
            const uint64_t relation_count = read_pod_mmap<uint64_t>(ptr);
            std::cout << "Loading " << relation_count << " relations..." << std::endl;
            
            relations_.reserve(relation_count);
            for (uint64_t i = 0; i < relation_count; ++i) {
                Relation relation;
                relation.osm_id = read_pod_mmap<int64_t>(ptr);
                relation.tags = read_tags_mmap(ptr);
                
                const uint32_t member_count = read_pod_mmap<uint32_t>(ptr);
                relation.member_ids.reserve(member_count);
                relation.member_types.reserve(member_count);
                relation.member_roles.reserve(member_count);
                
                for (uint32_t j = 0; j < member_count; ++j) {
                    relation.member_ids.push_back(read_pod_mmap<int64_t>(ptr));
                    relation.member_types.push_back(read_pod_mmap<uint8_t>(ptr));
                    relation.member_roles.push_back(read_string_mmap(ptr));
                }
                
                relations_.push_back(relation);
            }
        }
        
        // Build spatial indexes for POIs and features
        build_spatial_indexes();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Memory-mapped OSM loading completed in " << duration.count() << "ms" << std::endl;
        
        return true;
        
    } catch (const std::exception& ex) {
        std::cerr << "Error loading OSM file: " << ex.what() << std::endl;
        pois_.clear();
        return false;
    }
}

bool BinaryDatabase::load_with_cache(const std::string& streets_path,
                                     const std::string& osm_path,
                                     const std::string& cache_path,
                                     CacheManager& cache_manager) {
    const auto validation = cache_manager.validate_cache(cache_path, streets_path, osm_path);

    if (validation.valid) {
        if (load_from_cache(cache_path, cache_manager)) {
            // Validate R-tree structures after loading from cache
            std::cout << "Validating R-tree structures after cache load..." << std::endl;
            bool all_valid = true;
            if (!street_rtree_.validate_structure()) {
                std::cerr << "Street R-tree validation failed after cache load!" << std::endl;
                all_valid = false;
            }
            if (!intersection_rtree_.validate_structure()) {
                std::cerr << "Intersection R-tree validation failed after cache load!" << std::endl;
                all_valid = false;
            }
            if (!poi_rtree_.validate_structure()) {
                std::cerr << "POI R-tree validation failed after cache load!" << std::endl;
                all_valid = false;
            }
            if (!feature_rtree_.validate_structure()) {
                std::cerr << "Feature R-tree validation failed after cache load!" << std::endl;
                all_valid = false;
            }
            if (all_valid) {
                std::cout << "All R-tree structures validated successfully after cache load!" << std::endl;
            } else {
                std::cerr << "R-tree validation failed after cache load! Deleting cache and rebuilding..." << std::endl;
                cache_manager.delete_cache(cache_path);
                // Fall through to rebuild from source files
                return load_with_cache(streets_path, osm_path, cache_path, cache_manager);
            }
            return true;
        }
        
        // Enhanced error handling for cache load failure
        const auto& config = cache_manager.get_error_handling_config();
        if (config.enable_auto_recovery) {
            std::cerr << "Cache load failed, attempting recovery..." << std::endl;
            if (cache_manager.attempt_cache_recovery(cache_path, *this)) {
                std::cerr << "Cache recovery successful" << std::endl;
                return true;
            }
        }
        
        std::cerr << "Cache load failed, falling back to binary load" << std::endl;
    } else if (validation.exists) {
        std::cerr << "Cache invalid: " << validation.reason;
        if (!validation.detailed_error.empty()) {
            std::cerr << " (" << validation.detailed_error << ")";
        }
        std::cerr << std::endl;
        
        // Handle specific error types
        const auto& config = cache_manager.get_error_handling_config();
        if (config.enable_cache_cleanup) {
            switch (validation.error_type) {
                case CacheManager::ValidationResult::ErrorType::FileCorrupted:
                case CacheManager::ValidationResult::ErrorType::VersionMismatch:
                    std::cerr << "Deleting corrupted cache file..." << std::endl;
                    cache_manager.delete_cache(cache_path);
                    break;
                case CacheManager::ValidationResult::ErrorType::ChecksumMismatch:
                    std::cerr << "Source files changed, cache will be regenerated" << std::endl;
                    break;
                default:
                    break;
            }
        }
    } else {
        std::cerr << "Cache file not found, will create new cache" << std::endl;
    }

    if (!load_streets_file(streets_path)) {
        return false;
    }

    if (!load_osm_file(osm_path)) {
        return false;
    }

    // Build spatial indexes
    build_spatial_indexes();

    // Save cache for next time
    const auto streets_checksum = cache_manager.compute_file_checksum(streets_path);
    const auto osm_checksum = cache_manager.compute_file_checksum(osm_path);
    
    if (!save_to_cache(cache_path, cache_manager, streets_checksum, osm_checksum)) {
        std::cerr << "Failed to save cache (non-critical)" << std::endl;
    } else {
        std::cerr << "Cache saved successfully" << std::endl;
    }

    return true;
}

bool BinaryDatabase::load_from_cache(const std::string& cache_path,
                                     CacheManager& cache_manager) {
    clear();
    return cache_manager.load_cache(cache_path, *this);
}

bool BinaryDatabase::save_to_cache(const std::string& cache_path,
                                   CacheManager& cache_manager,
                                   const std::string& streets_checksum,
                                   const std::string& osm_checksum) const {
    return cache_manager.save_cache(cache_path, *this, streets_checksum, osm_checksum);
}

bool BinaryDatabase::load_with_cache(const std::string& streets_path,
                                     const std::string& osm_path) {
    // Create CacheManager with default error handling configuration
    CacheManager::ErrorHandlingConfig config;
    config.enable_auto_recovery = true;
    config.enable_corruption_detection = true;
    config.enable_version_validation = true;
    config.enable_checksum_validation = true;
    config.enable_fallback_loading = true;
    config.enable_cache_cleanup = true;
    config.max_retry_attempts = 3;
    config.corruption_threshold_bytes = 1024;
    
    CacheManager cache_manager(config);
    const std::string cache_path = generate_cache_path(streets_path, osm_path);
    return load_with_cache(streets_path, osm_path, cache_path, cache_manager);
}

std::string BinaryDatabase::generate_cache_path(const std::string& streets_path, const std::string& osm_path) const {
    // Generate cache path based on the streets file path
    // Replace the streets file extension with .gisevo.cache
    std::filesystem::path streets_file_path(streets_path);
    std::filesystem::path cache_path = streets_file_path.parent_path() / 
                                      (streets_file_path.stem().string() + ".gisevo.cache");
    return cache_path.string();
}

// Fallback stream-based OSM loading for compatibility
bool BinaryDatabase::load_osm_file_stream(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    
    try {
        // Read and verify magic
        char magic[8];
        in.read(magic, 8);
        if (std::memcmp(magic, "GISEVOO1", 8) != 0 && std::memcmp(magic, "GISEVOO2", 8) != 0) {
            throw std::runtime_error("Invalid osm.bin magic header");
        }
        
        // Read version
        const uint32_t version = read_pod<uint32_t>(in);
        if (version != 1 && version != 2) {
            throw std::runtime_error("Unsupported osm.bin version");
        }
        
        // Read POI count
        const uint64_t poi_count = read_pod<uint64_t>(in);
        
        // Read POIs
        pois_.reserve(poi_count);
        for (uint64_t i = 0; i < poi_count; ++i) {
            POI poi;
            poi.osm_id = read_pod<int64_t>(in);
            poi.lat = read_pod<double>(in);
            poi.lon = read_pod<double>(in);
            poi.category = read_string(in);
            poi.name = read_string(in);
            if (version == 2) {
                poi.tags = read_tags(in);
            }
            pois_.push_back(poi);
        }
        
        // Read feature count
        const uint64_t feature_count = read_pod<uint64_t>(in);
        
        // Read features
        features_.reserve(feature_count);
        for (uint64_t i = 0; i < feature_count; ++i) {
            Feature feature;
            feature.osm_id = read_pod<int64_t>(in);
            feature.type = static_cast<FeatureType>(read_pod<uint8_t>(in));
            feature.name = read_string(in);
            
            const uint64_t node_ref_count = read_pod<uint64_t>(in);
            feature.node_refs.reserve(node_ref_count);
            for (uint64_t j = 0; j < node_ref_count; ++j) {
                feature.node_refs.push_back(read_pod<int64_t>(in));
            }
            
            feature.is_closed = read_pod<bool>(in);
            if (version == 2) {
                feature.tags = read_tags(in);
            }
            features_.push_back(feature);
        }
        
        // Read relations (version 2 only)
        if (version == 2) {
            const uint64_t relation_count = read_pod<uint64_t>(in);
            relations_.reserve(relation_count);
            for (uint64_t i = 0; i < relation_count; ++i) {
                Relation relation;
                relation.osm_id = read_pod<int64_t>(in);
                relation.tags = read_tags(in);
                
                const uint32_t member_count = read_pod<uint32_t>(in);
                relation.member_ids.reserve(member_count);
                relation.member_types.reserve(member_count);
                relation.member_roles.reserve(member_count);
                
                for (uint32_t j = 0; j < member_count; ++j) {
                    relation.member_ids.push_back(read_pod<int64_t>(in));
                    relation.member_types.push_back(read_pod<uint8_t>(in));
                    relation.member_roles.push_back(read_string(in));
                }
                
                relations_.push_back(relation);
            }
        }
        
        // Build spatial indexes for POIs and features
        build_spatial_indexes();
        
        return true;
        
    } catch (const std::exception& ex) {
        pois_.clear();
        return false;
    }
}

void BinaryDatabase::clear() {
    nodes_.clear();
    segments_.clear();
    pois_.clear();
    features_.clear();
    relations_.clear();
    node_id_to_index_.clear();
    way_id_to_segment_index_.clear();
    relation_id_to_index_.clear();
    street_name_to_id_.clear();
    intersection_segments_.clear();
    intersection_node_ids_.clear();
}

LatLon BinaryDatabase::get_node_position(std::size_t index) const {
    if (index >= nodes_.size()) {
        return LatLon(0, 0);
    }
    return LatLon(nodes_[index].lat, nodes_[index].lon);
}

OSMID BinaryDatabase::get_node_osm_id(std::size_t index) const {
    if (index >= nodes_.size()) {
        return 0;
    }
    return nodes_[index].osm_id;
}

const BinaryDatabase::Node& BinaryDatabase::get_node_by_index(std::size_t index) const {
    if (index >= nodes_.size()) {
        static const Node empty_node{};
        return empty_node;
    }
    return nodes_[index];
}

const std::vector<std::pair<std::string, std::string>>& BinaryDatabase::get_node_tags(std::size_t index) const {
    if (index >= nodes_.size()) {
        static const std::vector<std::pair<std::string, std::string>> empty_tags;
        return empty_tags;
    }
    return nodes_[index].tags;
}

StreetSegmentInfo BinaryDatabase::get_segment_info(std::size_t index) const {
    StreetSegmentInfo info = {};
    if (index >= segments_.size()) {
        return info;
    }
    
    const auto& seg = segments_[index];
    
    // Find intersection indices for from/to
    auto from_it = node_id_to_index_.find(seg.node_refs.front());
    auto to_it = node_id_to_index_.find(seg.node_refs.back());
    
    if (from_it != node_id_to_index_.end() && to_it != node_id_to_index_.end()) {
        info.from = static_cast<IntersectionIdx>(from_it->second);
        info.to = static_cast<IntersectionIdx>(to_it->second);
    }
    
    // Street ID - for now, just use segment index (simplified)
    info.streetID = static_cast<StreetIdx>(index);
    info.wayOSMID = seg.osm_id;
    info.numCurvePoints = seg.node_refs.size() - 2; // Exclude endpoints
    info.oneWay = false; // TODO: determine from OSM data
    info.speedLimit = seg.max_speed_kph;
    
    return info;
}

const BinaryDatabase::StreetSegment& BinaryDatabase::get_segment_by_index(std::size_t index) const {
    if (index >= segments_.size()) {
        static const StreetSegment empty_segment{};
        return empty_segment;
    }
    return segments_[index];
}

const std::vector<std::pair<std::string, std::string>>& BinaryDatabase::get_segment_tags(std::size_t index) const {
    if (index >= segments_.size()) {
        static const std::vector<std::pair<std::string, std::string>> empty_tags;
        return empty_tags;
    }
    return segments_[index].tags;
}

const std::vector<OSMID>& BinaryDatabase::get_segment_node_refs(std::size_t index) const {
    if (index >= segments_.size()) {
        static const std::vector<OSMID> empty_refs;
        return empty_refs;
    }
    return segments_[index].node_refs;
}

std::size_t BinaryDatabase::get_street_count() const {
    return street_name_to_id_.size();
}

std::string BinaryDatabase::get_street_name(std::size_t street_id) const {
    // Simplified: return segment name for now
    if (street_id < segments_.size()) {
        return segments_[street_id].name;
    }
    return "";
}

std::size_t BinaryDatabase::get_intersection_count() const {
    return intersection_node_ids_.size();
}

LatLon BinaryDatabase::get_intersection_position(std::size_t idx) const {
    if (idx >= intersection_node_ids_.size()) {
        return LatLon(0, 0);
    }
    
    OSMID node_id = intersection_node_ids_[idx];
    auto it = node_id_to_index_.find(node_id);
    if (it != node_id_to_index_.end()) {
        return get_node_position(it->second);
    }
    return LatLon(0, 0);
}

std::string BinaryDatabase::get_intersection_name(std::size_t idx) const {
    // TODO: Generate intersection names from connected streets
    return "Intersection " + std::to_string(idx);
}

OSMID BinaryDatabase::get_intersection_osm_node_id(std::size_t idx) const {
    if (idx >= intersection_node_ids_.size()) {
        return 0;
    }
    return intersection_node_ids_[idx];
}

LatLon BinaryDatabase::get_poi_position(std::size_t idx) const {
    if (idx >= pois_.size()) {
        return LatLon(0, 0);
    }
    return LatLon(pois_[idx].lat, pois_[idx].lon);
}

std::string BinaryDatabase::get_poi_name(std::size_t idx) const {
    if (idx >= pois_.size()) {
        return "";
    }
    return pois_[idx].name;
}

std::string BinaryDatabase::get_poi_type(std::size_t idx) const {
    if (idx >= pois_.size()) {
        return "";
    }
    return pois_[idx].category;
}

OSMID BinaryDatabase::get_poi_osm_id(std::size_t idx) const {
    if (idx >= pois_.size()) {
        return 0;
    }
    return pois_[idx].osm_id;
}

unsigned BinaryDatabase::get_intersection_street_segment_count(std::size_t intersection_idx) const {
    if (intersection_idx >= intersection_segments_.size()) {
        return 0;
    }
    return static_cast<unsigned>(intersection_segments_[intersection_idx].size());
}

StreetSegmentIdx BinaryDatabase::get_intersection_street_segment(std::size_t street_segment_num, std::size_t intersection_idx) const {
    if (intersection_idx >= intersection_segments_.size() || 
        street_segment_num >= intersection_segments_[intersection_idx].size()) {
        return 0;
    }
    return static_cast<StreetSegmentIdx>(intersection_segments_[intersection_idx][street_segment_num]);
}

void BinaryDatabase::build_indexes() {
    std::cout << "Building spatial indexes..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Calculate map boundaries from all loaded nodes
    if (!nodes_.empty()) {
        min_lat_ = max_lat_ = nodes_[0].lat;
        min_lon_ = max_lon_ = nodes_[0].lon;
        
        double lat_sum = 0.0;
        for (const auto& node : nodes_) {
            min_lat_ = std::min(min_lat_, node.lat);
            max_lat_ = std::max(max_lat_, node.lat);
            min_lon_ = std::min(min_lon_, node.lon);
            max_lon_ = std::max(max_lon_, node.lon);
            lat_sum += node.lat;
        }
        
        // Calculate average latitude in radians
        avg_lat_rad_ = (lat_sum / nodes_.size()) * (M_PI / 180.0);
        
        std::cout << "Map boundaries: lat=[" << min_lat_ << ", " << max_lat_ 
                  << "], lon=[" << min_lon_ << ", " << max_lon_ 
                  << "], avg_lat_rad=" << avg_lat_rad_ << std::endl;
    }
    
    // Build node ID lookup
    node_id_to_index_.clear();
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        node_id_to_index_[nodes_[i].osm_id] = i;
    }
    
    // Build way ID to segment index lookup
    way_id_to_segment_index_.clear();
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        way_id_to_segment_index_[segments_[i].osm_id] = i;
    }
    
    // Build relation ID to index lookup
    relation_id_to_index_.clear();
    for (std::size_t i = 0; i < relations_.size(); ++i) {
        relation_id_to_index_[relations_[i].osm_id] = i;
    }
    
    // Build street name lookup (simplified - each segment gets its own "street" for now)
    street_name_to_id_.clear();
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        const auto& name = segments_[i].name;
        if (!name.empty() && street_name_to_id_.find(name) == street_name_to_id_.end()) {
            street_name_to_id_[name] = i;
        }
    }
    
    // Build intersection structures
    // Count how many segments use each node
    std::unordered_map<OSMID, std::vector<std::size_t>> node_to_segments;
    
    for (std::size_t seg_idx = 0; seg_idx < segments_.size(); ++seg_idx) {
        const auto& seg = segments_[seg_idx];
        if (seg.node_refs.size() >= 2) {
            // First and last nodes are potential intersections
            node_to_segments[seg.node_refs.front()].push_back(seg_idx);
            node_to_segments[seg.node_refs.back()].push_back(seg_idx);
        }
    }
    
    // Nodes with multiple segments are intersections
    intersection_node_ids_.clear();
    intersection_segments_.clear();
    
    for (const auto& [node_id, seg_indices] : node_to_segments) {
        if (seg_indices.size() >= 2) {
            // This is an intersection
            intersection_node_ids_.push_back(node_id);
            intersection_segments_.push_back(seg_indices);
        }
    }
    
    // Build R-tree spatial indexes
    try {
        build_spatial_indexes();
    } catch (const std::exception& ex) {
        std::cerr << "Error building spatial indexes: " << ex.what() << std::endl;
        std::cerr.flush();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Spatial indexes built in " << duration.count() << "ms" << std::endl;
}

void BinaryDatabase::build_spatial_indexes() {
    // Build street segment spatial index
    std::vector<std::pair<std::size_t, BoundingBox>> street_entries;
    street_entries.reserve(segments_.size());
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        const auto& seg = segments_[i];
        if (seg.node_refs.size() < 2) {
            continue;
        }

        auto from_it = node_id_to_index_.find(seg.node_refs.front());
        auto to_it = node_id_to_index_.find(seg.node_refs.back());
        if (from_it == node_id_to_index_.end() || to_it == node_id_to_index_.end()) {
            continue;
        }

        const auto& from_node = nodes_[from_it->second];
        const auto& to_node = nodes_[to_it->second];

        BoundingBox bounds(
            std::min(from_node.lon, to_node.lon),
            std::min(from_node.lat, to_node.lat),
            std::max(from_node.lon, to_node.lon),
            std::max(from_node.lat, to_node.lat)
        );

        street_entries.emplace_back(i, bounds);
    }
    street_rtree_.bulk_load(std::move(street_entries));

    // Build intersection R-tree
    std::vector<std::pair<std::size_t, BoundingBox>> intersection_entries;
    intersection_entries.reserve(intersection_node_ids_.size());
    for (std::size_t i = 0; i < intersection_node_ids_.size(); ++i) {
        OSMID node_id = intersection_node_ids_[i];
        auto it = node_id_to_index_.find(node_id);
        if (it == node_id_to_index_.end()) {
            continue;
        }

        const auto& node = nodes_[it->second];
        BoundingBox bounds(node.lon, node.lat, node.lon, node.lat);
        intersection_entries.emplace_back(i, bounds);
    }
    intersection_rtree_.bulk_load(std::move(intersection_entries));

    // Build POI R-tree
    std::vector<std::pair<std::size_t, BoundingBox>> poi_entries;
    poi_entries.reserve(pois_.size());
    for (std::size_t i = 0; i < pois_.size(); ++i) {
        const auto& poi = pois_[i];
        BoundingBox bounds(poi.lon, poi.lat, poi.lon, poi.lat);
        poi_entries.emplace_back(i, bounds);
    }
    poi_rtree_.bulk_load(std::move(poi_entries));

    // Build feature R-tree
    std::vector<std::pair<std::size_t, BoundingBox>> feature_entries;
    feature_entries.reserve(features_.size());
    for (std::size_t i = 0; i < features_.size(); ++i) {
        const auto& feature = features_[i];
        if (feature.node_refs.empty()) {
            continue;
        }

        double min_lon = std::numeric_limits<double>::max();
        double min_lat = std::numeric_limits<double>::max();
        double max_lon = std::numeric_limits<double>::lowest();
        double max_lat = std::numeric_limits<double>::lowest();
        bool has_node = false;

        for (OSMID node_id : feature.node_refs) {
            auto it = node_id_to_index_.find(node_id);
            if (it == node_id_to_index_.end()) {
                continue;
            }

            const auto& node = nodes_[it->second];
            min_lon = std::min(min_lon, node.lon);
            min_lat = std::min(min_lat, node.lat);
            max_lon = std::max(max_lon, node.lon);
            max_lat = std::max(max_lat, node.lat);
            has_node = true;
        }

        if (!has_node) {
            continue;
        }

        BoundingBox bounds(min_lon, min_lat, max_lon, max_lat);
        feature_entries.emplace_back(i, bounds);
    }
    feature_rtree_.bulk_load(std::move(feature_entries));
    
    std::cout << "R-tree sizes: streets=" << street_rtree_.size() 
              << ", intersections=" << intersection_rtree_.size()
              << ", pois=" << poi_rtree_.size()
              << ", features=" << feature_rtree_.size() << std::endl;
    
    std::cout << "R-tree depths: streets=" << street_rtree_.depth() 
              << ", intersections=" << intersection_rtree_.depth()
              << ", pois=" << poi_rtree_.depth()
              << ", features=" << feature_rtree_.depth() << std::endl;
    
    // Validate R-tree structures
    std::cout << "Validating R-tree structures..." << std::endl;
    bool all_valid = true;
    if (!street_rtree_.validate_structure()) {
        std::cerr << "Street R-tree validation failed!" << std::endl;
        all_valid = false;
    }
    if (!intersection_rtree_.validate_structure()) {
        std::cerr << "Intersection R-tree validation failed!" << std::endl;
        all_valid = false;
    }
    if (!poi_rtree_.validate_structure()) {
        std::cerr << "POI R-tree validation failed!" << std::endl;
        all_valid = false;
    }
    if (!feature_rtree_.validate_structure()) {
        std::cerr << "Feature R-tree validation failed!" << std::endl;
        all_valid = false;
    }
    if (all_valid) {
        std::cout << "All R-tree structures validated successfully!" << std::endl;
    }
}

// Feature query implementations
FeatureType BinaryDatabase::get_feature_type(std::size_t idx) const {
    if (idx >= features_.size()) {
        return FeatureType::UNKNOWN;
    }
    return features_[idx].type;
}

OSMID BinaryDatabase::get_feature_osm_id(std::size_t idx) const {
    if (idx >= features_.size()) {
        return 0;
    }
    return features_[idx].osm_id;
}

std::string BinaryDatabase::get_feature_name(std::size_t idx) const {
    if (idx >= features_.size()) {
        return "";
    }
    return features_[idx].name;
}

std::size_t BinaryDatabase::get_feature_point_count(std::size_t idx) const {
    if (idx >= features_.size()) {
        return 0;
    }
    return features_[idx].node_refs.size();
}

LatLon BinaryDatabase::get_feature_point(std::size_t point_idx, std::size_t feature_idx) const {
    if (feature_idx >= features_.size() || point_idx >= features_[feature_idx].node_refs.size()) {
        return LatLon(0, 0);
    }
    
    const OSMID node_id = features_[feature_idx].node_refs[point_idx];
    const auto it = node_id_to_index_.find(node_id);
    if (it == node_id_to_index_.end()) {
        return LatLon(0, 0);
    }
    
    const Node& node = nodes_[it->second];
    return LatLon(node.lat, node.lon);
}

// Spatial query implementations
std::vector<std::size_t> BinaryDatabase::query_streets_in_bounds(double min_x, double min_y, double max_x, double max_y) const {
    return street_rtree_.query(min_x, min_y, max_x, max_y);
}

std::vector<std::size_t> BinaryDatabase::query_intersections_in_bounds(double min_x, double min_y, double max_x, double max_y) const {
    return intersection_rtree_.query(min_x, min_y, max_x, max_y);
}

std::vector<std::size_t> BinaryDatabase::query_pois_in_bounds(double min_x, double min_y, double max_x, double max_y) const {
    return poi_rtree_.query(min_x, min_y, max_x, max_y);
}

std::vector<std::size_t> BinaryDatabase::query_features_in_bounds(double min_x, double min_y, double max_x, double max_y) const {
    return feature_rtree_.query(min_x, min_y, max_x, max_y);
}

// Relation query implementations
const BinaryDatabase::Relation& BinaryDatabase::get_relation_by_index(std::size_t idx) const {
    if (idx >= relations_.size()) {
        static const Relation empty_relation{};
        return empty_relation;
    }
    return relations_[idx];
}

const BinaryDatabase::Relation* BinaryDatabase::get_relation_by_osm_id(OSMID osm_id) const {
    const auto it = relation_id_to_index_.find(osm_id);
    if (it == relation_id_to_index_.end()) {
        return nullptr;
    }
    return &relations_[it->second];
}

const std::vector<std::pair<std::string, std::string>>& BinaryDatabase::get_relation_tags(std::size_t index) const {
    if (index >= relations_.size()) {
        static const std::vector<std::pair<std::string, std::string>> empty_tags;
        return empty_tags;
    }
    return relations_[index].tags;
}

const std::vector<OSMID>& BinaryDatabase::get_relation_member_ids(std::size_t index) const {
    if (index >= relations_.size()) {
        static const std::vector<OSMID> empty_ids;
        return empty_ids;
    }
    return relations_[index].member_ids;
}

const std::vector<std::uint8_t>& BinaryDatabase::get_relation_member_types(std::size_t index) const {
    if (index >= relations_.size()) {
        static const std::vector<std::uint8_t> empty_types;
        return empty_types;
    }
    return relations_[index].member_types;
}

const std::vector<std::string>& BinaryDatabase::get_relation_member_roles(std::size_t index) const {
    if (index >= relations_.size()) {
        static const std::vector<std::string> empty_roles;
        return empty_roles;
    }
    return relations_[index].member_roles;
}

LatLon BinaryDatabase::get_street_segment_curve_point(std::size_t curve_point_num, std::size_t street_segment_idx) const {
    if (street_segment_idx >= segments_.size()) {
        return LatLon(0, 0);
    }
    
    const auto& segment = segments_[street_segment_idx];
    
    // Curve points are the intermediate nodes between start and end intersections
    // The node_refs array contains: [start_intersection, curve_point_1, curve_point_2, ..., end_intersection]
    // So curve_point_num 0 corresponds to node_refs[1], curve_point_num 1 to node_refs[2], etc.
    std::size_t node_ref_idx = curve_point_num + 1;
    
    if (node_ref_idx >= segment.node_refs.size()) {
        return LatLon(0, 0);
    }
    
    OSMID node_id = segment.node_refs[node_ref_idx];
    auto it = node_id_to_index_.find(node_id);
    if (it == node_id_to_index_.end()) {
        return LatLon(0, 0);
    }
    
    const Node& node = nodes_[it->second];
    return LatLon(node.lat, node.lon);
}

// Cache serialization support methods
void BinaryDatabase::set_map_bounds(double min_lat, double max_lat, double min_lon, double max_lon, double avg_lat_rad) {
    min_lat_ = min_lat;
    max_lat_ = max_lat;
    min_lon_ = min_lon;
    max_lon_ = max_lon;
    avg_lat_rad_ = avg_lat_rad;
}

void BinaryDatabase::add_node(const Node& node) {
    nodes_.push_back(node);
    node_id_to_index_[node.osm_id] = nodes_.size() - 1;
}

void BinaryDatabase::add_segment(const StreetSegment& segment) {
    segments_.push_back(segment);
    way_id_to_segment_index_[segment.osm_id] = segments_.size() - 1;
}

void BinaryDatabase::add_poi(const POI& poi) {
    pois_.push_back(poi);
}

void BinaryDatabase::add_feature(const Feature& feature) {
    features_.push_back(feature);
}

void BinaryDatabase::add_relation(const Relation& relation) {
    relations_.push_back(relation);
    relation_id_to_index_[relation.osm_id] = relations_.size() - 1;
}

void BinaryDatabase::set_lookup_maps(const std::unordered_map<OSMID, std::size_t>& node_map,
                                    const std::unordered_map<OSMID, std::size_t>& way_map,
                                    const std::unordered_map<OSMID, std::size_t>& relation_map,
                                    const std::unordered_map<std::string, std::size_t>& street_map) {
    node_id_to_index_ = node_map;
    way_id_to_segment_index_ = way_map;
    relation_id_to_index_ = relation_map;
    street_name_to_id_ = street_map;
}

void BinaryDatabase::set_intersection_data(const std::vector<std::vector<std::size_t>>& intersection_segments,
                                         const std::vector<OSMID>& intersection_node_ids) {
    intersection_segments_ = intersection_segments;
    intersection_node_ids_ = intersection_node_ids;
}

void BinaryDatabase::rebuild_spatial_indexes() {
    build_spatial_indexes();
}

void BinaryDatabase::deserialize_street_rtree(std::istream& in) {
    street_rtree_.clear();
    street_rtree_.deserialize(in);
}

void BinaryDatabase::deserialize_intersection_rtree(std::istream& in) {
    intersection_rtree_.clear();
    intersection_rtree_.deserialize(in);
}

void BinaryDatabase::deserialize_poi_rtree(std::istream& in) {
    poi_rtree_.clear();
    poi_rtree_.deserialize(in);
}

void BinaryDatabase::deserialize_feature_rtree(std::istream& in) {
    feature_rtree_.clear();
    feature_rtree_.deserialize(in);
}

} // namespace gisevo