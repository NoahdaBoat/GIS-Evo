#pragma once

#include "StreetsDatabaseAPI.h"
#include "OSMDatabaseAPI.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../spatial_hash/simple_spatial_index.hpp"

namespace gisevo {

// In-memory representation of loaded map data
class BinaryDatabase {
public:
    static BinaryDatabase& instance();
    
    // Internal data structures (public for API access)
    struct Node {
        OSMID osm_id;
        double lat;
        double lon;
        std::vector<std::pair<std::string, std::string>> tags;
    };
    
    struct StreetSegment {
        OSMID osm_id;
        uint8_t category;
        float max_speed_kph;
        std::string name;
        std::vector<OSMID> node_refs;  // Changed from int64_t to OSMID
        bool is_closed;
        std::vector<std::pair<std::string, std::string>> tags;
    };
    
    struct POI {
        OSMID osm_id;
        double lat;
        double lon;
        std::string category;
        std::string name;
        std::vector<std::pair<std::string, std::string>> tags;
    };
    
    struct Feature {
        OSMID osm_id;
        FeatureType type;
        std::string name;
        std::vector<OSMID> node_refs;
        bool is_closed;
        std::vector<std::pair<std::string, std::string>> tags;
    };
    
    struct Relation {
        OSMID osm_id;
        std::vector<std::pair<std::string, std::string>> tags;
        std::vector<OSMID> member_ids;
        std::vector<std::uint8_t> member_types;  // 0=Node, 1=Way, 2=Relation
        std::vector<std::string> member_roles;
    };
    
    // Database loading
    bool load_streets_file(const std::string& path);
    bool load_osm_file(const std::string& path);
    void clear();
    
    // Fallback stream loading for compatibility
    bool load_streets_file_stream(const std::string& path);
    bool load_osm_file_stream(const std::string& path);
    
    // Node queries
    std::size_t get_node_count() const { return nodes_.size(); }
    LatLon get_node_position(std::size_t index) const;
    OSMID get_node_osm_id(std::size_t index) const;
    const Node& get_node_by_index(std::size_t index) const;
    const std::vector<std::pair<std::string, std::string>>& get_node_tags(std::size_t index) const;
    
    // Street segment queries  
    std::size_t get_segment_count() const { return segments_.size(); }
    StreetSegmentInfo get_segment_info(std::size_t index) const;
    const StreetSegment& get_segment_by_index(std::size_t index) const;
    const std::vector<std::pair<std::string, std::string>>& get_segment_tags(std::size_t index) const;
    const std::vector<OSMID>& get_segment_node_refs(std::size_t index) const;
    
    // Street queries
    std::size_t get_street_count() const;
    std::string get_street_name(std::size_t street_id) const;
    
    // Intersection queries
    std::size_t get_intersection_count() const;
    LatLon get_intersection_position(std::size_t idx) const;
    std::string get_intersection_name(std::size_t idx) const;
    OSMID get_intersection_osm_node_id(std::size_t idx) const;
    
    // POI queries
    std::size_t get_poi_count() const { return pois_.size(); }
    LatLon get_poi_position(std::size_t idx) const;
    std::string get_poi_name(std::size_t idx) const;
    std::string get_poi_type(std::size_t idx) const;
    
    // Feature queries
    std::size_t get_feature_count() const { return features_.size(); }
    FeatureType get_feature_type(std::size_t idx) const;
    OSMID get_feature_osm_id(std::size_t idx) const;
    std::string get_feature_name(std::size_t idx) const;
    std::size_t get_feature_point_count(std::size_t idx) const;
    LatLon get_feature_point(std::size_t point_idx, std::size_t feature_idx) const;
    
    // Relation queries
    std::size_t get_relation_count() const { return relations_.size(); }
    const Relation& get_relation_by_index(std::size_t idx) const;
    const Relation* get_relation_by_osm_id(OSMID osm_id) const;
    const std::vector<std::pair<std::string, std::string>>& get_relation_tags(std::size_t index) const;
    const std::vector<OSMID>& get_relation_member_ids(std::size_t index) const;
    const std::vector<std::uint8_t>& get_relation_member_types(std::size_t index) const;
    const std::vector<std::string>& get_relation_member_roles(std::size_t index) const;
    
    // Additional queries needed by API
    unsigned get_intersection_street_segment_count(std::size_t intersection_idx) const;
    StreetSegmentIdx get_intersection_street_segment(std::size_t street_segment_num, std::size_t intersection_idx) const;
    
    // Lookup map accessors
    const std::unordered_map<OSMID, std::size_t>& get_way_id_to_segment_index() const { return way_id_to_segment_index_; }
    const std::unordered_map<OSMID, std::size_t>& get_node_id_to_index() const { return node_id_to_index_; }
    
    // Spatial queries using R-tree
    std::vector<std::size_t> query_streets_in_bounds(double min_x, double min_y, double max_x, double max_y) const;
    std::vector<std::size_t> query_intersections_in_bounds(double min_x, double min_y, double max_x, double max_y) const;
    std::vector<std::size_t> query_pois_in_bounds(double min_x, double min_y, double max_x, double max_y) const;
    std::vector<std::size_t> query_features_in_bounds(double min_x, double min_y, double max_x, double max_y) const;
    
    // Map boundary functions
    double get_min_lat() const { return min_lat_; }
    double get_max_lat() const { return max_lat_; }
    double get_min_lon() const { return min_lon_; }
    double get_max_lon() const { return max_lon_; }
    double get_avg_lat_rad() const { return avg_lat_rad_; }
    
    // Street segment curve point function
    LatLon get_street_segment_curve_point(std::size_t curve_point_num, std::size_t street_segment_idx) const;

private:
    BinaryDatabase() = default;
    
    std::vector<Node> nodes_;
    std::vector<StreetSegment> segments_;
    std::vector<POI> pois_;
    std::vector<Feature> features_;
    std::vector<Relation> relations_;
    
    // Derived/indexed data (built after loading)
    std::unordered_map<OSMID, std::size_t> node_id_to_index_;
    std::unordered_map<OSMID, std::size_t> way_id_to_segment_index_;
    std::unordered_map<OSMID, std::size_t> relation_id_to_index_;
    std::unordered_map<std::string, std::size_t> street_name_to_id_;
    std::vector<std::vector<std::size_t>> intersection_segments_; // segments per intersection
    std::vector<OSMID> intersection_node_ids_; // OSM node IDs for intersections
    
    // Memory-mapped file support
    struct MappedFile {
        void* data = nullptr;
        size_t size = 0;
        int fd = -1;
        
        ~MappedFile() {
            if (data && data != MAP_FAILED) {
                munmap(data, size);
            }
            if (fd != -1) {
                close(fd);
            }
        }
    };
    
    std::unique_ptr<MappedFile> streets_mmap_;
    std::unique_ptr<MappedFile> osm_mmap_;
    
    // Spatial indexes for fast queries
    RTree<std::size_t> street_rtree_;
    RTree<std::size_t> intersection_rtree_;
    RTree<std::size_t> poi_rtree_;
    RTree<std::size_t> feature_rtree_;
    
    // Map boundary values
    double min_lat_ = 0.0;
    double max_lat_ = 0.0;
    double min_lon_ = 0.0;
    double max_lon_ = 0.0;
    double avg_lat_rad_ = 0.0;
    
    void build_indexes();
    void build_spatial_indexes();
    bool map_file(const std::string& path, std::unique_ptr<MappedFile>& mmap);
};

} // namespace gisevo