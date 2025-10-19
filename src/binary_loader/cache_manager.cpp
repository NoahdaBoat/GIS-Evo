#include "cache_manager.hpp"

#include "binary_database.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace gisevo {

namespace {

constexpr std::size_t kMagicLength = 8;

bool read_exact(std::istream& in, char* buffer, std::size_t length) {
    in.read(buffer, static_cast<std::streamsize>(length));
    return in.good();
}

bool write_exact(std::ostream& out, const char* buffer, std::size_t length) {
    out.write(buffer, static_cast<std::streamsize>(length));
    return out.good();
}

bool write_string(std::ostream& out, const std::string& value) {
    const std::uint32_t size = static_cast<std::uint32_t>(value.size());
    out.write(reinterpret_cast<const char*>(&size), sizeof(size));
    out.write(value.data(), size);
    return out.good();
}

bool read_string(std::istream& in, std::string& value) {
    std::uint32_t size = 0;
    in.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!in.good()) {
        return false;
    }
    value.resize(size);
    in.read(value.data(), size);
    return in.good();
}

} // namespace

std::string CacheManager::CacheMetadata::streets_checksum_string() const {
    return std::string(streets_checksum, streets_checksum + kChecksumLength);
}

std::string CacheManager::CacheMetadata::osm_checksum_string() const {
    return std::string(osm_checksum, osm_checksum + kChecksumLength);
}

CacheManager::CacheManager() = default;

CacheManager::ValidationResult CacheManager::validate_cache(
    const std::string& cache_path,
    const std::string& streets_path,
    const std::string& osm_path) const {

    ValidationResult result;

    if (!std::filesystem::exists(cache_path)) {
        result.exists = false;
        result.valid = false;
        result.reason = "Cache file missing";
        return result;
    }

    result.exists = true;

    std::ifstream in(cache_path, std::ios::binary);
    if (!in.good()) {
        result.valid = false;
        result.reason = "Failed to open cache file";
        return result;
    }

    char magic[kMagicLength];
    if (!read_exact(in, magic, kMagicLength) || std::string(magic, kMagicLength) != kCacheMagic) {
        result.valid = false;
        result.reason = "Invalid cache magic";
        return result;
    }

    CacheMetadata metadata;
    if (!read_metadata(in, metadata)) {
        result.valid = false;
        result.reason = "Failed to read metadata";
        return result;
    }

    result.metadata = metadata;

    if (metadata.version != kCacheVersion) {
        result.valid = false;
        result.reason = "Cache version mismatch";
        return result;
    }

    const auto streets_checksum = compute_checksum_hex(streets_path);
    const auto osm_checksum = compute_checksum_hex(osm_path);

    result.streets_checksum = streets_checksum;
    result.osm_checksum = osm_checksum;

    if (streets_checksum.empty() || osm_checksum.empty()) {
        result.valid = false;
        result.reason = "Failed to compute source checksums";
        return result;
    }

    if (metadata.streets_checksum_string() != streets_checksum) {
        result.valid = false;
        result.reason = "Streets checksum mismatch";
        return result;
    }

    if (metadata.osm_checksum_string() != osm_checksum) {
        result.valid = false;
        result.reason = "OSM checksum mismatch";
        return result;
    }

    result.valid = true;
    result.reason.clear();
    return result;
}

bool CacheManager::load_cache(const std::string& cache_path, BinaryDatabase& db) const {
    std::ifstream in(cache_path, std::ios::binary);
    if (!in.good()) {
        return false;
    }

    char magic[kMagicLength];
    if (!read_exact(in, magic, kMagicLength) || std::string(magic, kMagicLength) != kCacheMagic) {
        return false;
    }

    CacheMetadata metadata;
    if (!read_metadata(in, metadata)) {
        return false;
    }

    try {
        // Deserialize BinaryDatabase data structures
        if (!deserialize_database(in, db, metadata)) {
            return false;
        }

        return true;
    } catch (const std::exception& ex) {
        std::cerr << "Error deserializing cache: " << ex.what() << std::endl;
        return false;
    }
}

bool CacheManager::save_cache(const std::string& cache_path,
                              const BinaryDatabase& db,
                              const std::string& streets_checksum,
                              const std::string& osm_checksum) const {
    if (!ensure_directory_exists(cache_path)) {
        return false;
    }

    std::ofstream out(cache_path, std::ios::binary | std::ios::trunc);
    if (!out.good()) {
        return false;
    }

    if (!write_exact(out, kCacheMagic, kMagicLength)) {
        return false;
    }

    CacheMetadata metadata;
    metadata.version = kCacheVersion;
    metadata.creation_timestamp = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    // Get map bounds from database
    metadata.min_lat = db.get_min_lat();
    metadata.max_lat = db.get_max_lat();
    metadata.min_lon = db.get_min_lon();
    metadata.max_lon = db.get_max_lon();
    metadata.avg_lat_rad = db.get_avg_lat_rad();

    std::snprintf(metadata.streets_checksum, CacheMetadata::kChecksumLength + 1, "%s", streets_checksum.c_str());
    std::snprintf(metadata.osm_checksum, CacheMetadata::kChecksumLength + 1, "%s", osm_checksum.c_str());

    if (!write_metadata(out, metadata)) {
        return false;
    }

    try {
        // Serialize BinaryDatabase data structures
        if (!serialize_database(out, db)) {
            return false;
        }

        return out.good();
    } catch (const std::exception& ex) {
        std::cerr << "Error serializing cache: " << ex.what() << std::endl;
        return false;
    }
}

CacheManager::CacheMetadata CacheManager::read_metadata(const std::string& cache_path) const {
    CacheMetadata metadata;
    std::ifstream in(cache_path, std::ios::binary);
    if (!in.good()) {
        return metadata;
    }

    char magic[kMagicLength];
    if (!read_exact(in, magic, kMagicLength) || std::string(magic, kMagicLength) != kCacheMagic) {
        return metadata;
    }

    if (!read_metadata(in, metadata)) {
        return CacheMetadata{};
    }

    return metadata;
}

std::string CacheManager::compute_checksum_hex(const std::string& file_path) const {
    std::ifstream in(file_path, std::ios::binary);
    if (!in.good()) {
        return {};
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    unsigned char buffer[4096];
    unsigned long long low = 0;
    unsigned long long high = 0;

    while (in.good()) {
        in.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
        const auto read_bytes = in.gcount();
        for (std::streamsize i = 0; i < read_bytes; ++i) {
            low += buffer[i];
            high += low;
        }
    }

    oss << std::setw(16) << (high & 0xFFFFFFFFFFFFull)
        << std::setw(16) << (low & 0xFFFFFFFFFFFFull);

    std::string hex = oss.str();
    if (hex.size() < CacheMetadata::kChecksumLength) {
        hex.append(CacheMetadata::kChecksumLength - hex.size(), '0');
    } else if (hex.size() > CacheMetadata::kChecksumLength) {
        hex.resize(CacheMetadata::kChecksumLength);
    }

    return hex;
}

bool CacheManager::write_metadata(std::ostream& out, const CacheMetadata& metadata) const {
    out.write(reinterpret_cast<const char*>(&metadata.version), sizeof(metadata.version));
    out.write(reinterpret_cast<const char*>(&metadata.creation_timestamp), sizeof(metadata.creation_timestamp));
    out.write(reinterpret_cast<const char*>(&metadata.min_lat), sizeof(metadata.min_lat));
    out.write(reinterpret_cast<const char*>(&metadata.max_lat), sizeof(metadata.max_lat));
    out.write(reinterpret_cast<const char*>(&metadata.min_lon), sizeof(metadata.min_lon));
    out.write(reinterpret_cast<const char*>(&metadata.max_lon), sizeof(metadata.max_lon));

    if (!write_string(out, metadata.streets_checksum_string())) {
        return false;
    }
    if (!write_string(out, metadata.osm_checksum_string())) {
        return false;
    }

    return out.good();
}

bool CacheManager::read_metadata(std::istream& in, CacheMetadata& metadata) const {
    in.read(reinterpret_cast<char*>(&metadata.version), sizeof(metadata.version));
    in.read(reinterpret_cast<char*>(&metadata.creation_timestamp), sizeof(metadata.creation_timestamp));
    in.read(reinterpret_cast<char*>(&metadata.min_lat), sizeof(metadata.min_lat));
    in.read(reinterpret_cast<char*>(&metadata.max_lat), sizeof(metadata.max_lat));
    in.read(reinterpret_cast<char*>(&metadata.min_lon), sizeof(metadata.min_lon));
    in.read(reinterpret_cast<char*>(&metadata.max_lon), sizeof(metadata.max_lon));
    if (!in.good()) {
        return false;
    }

    std::string streets_checksum;
    std::string osm_checksum;
    if (!read_string(in, streets_checksum) || !read_string(in, osm_checksum)) {
        return false;
    }

    streets_checksum.resize(CacheMetadata::kChecksumLength, '0');
    osm_checksum.resize(CacheMetadata::kChecksumLength, '0');
    std::memcpy(metadata.streets_checksum, streets_checksum.data(), CacheMetadata::kChecksumLength);
    std::memcpy(metadata.osm_checksum, osm_checksum.data(), CacheMetadata::kChecksumLength);

    return true;
}

bool CacheManager::ensure_directory_exists(const std::string& cache_path) const {
    const auto parent = std::filesystem::path(cache_path).parent_path();
    if (parent.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    return !ec;
}

bool CacheManager::serialize_database(std::ostream& out, const BinaryDatabase& db) const {
    // Serialize nodes
    const auto node_count = db.get_node_count();
    if (!write_vector_size(out, node_count)) return false;
    
    for (std::size_t i = 0; i < node_count; ++i) {
        const auto& node = db.get_node_by_index(i);
        
        // Write OSMID, lat, lon
        out.write(reinterpret_cast<const char*>(&node.osm_id), sizeof(node.osm_id));
        out.write(reinterpret_cast<const char*>(&node.lat), sizeof(node.lat));
        out.write(reinterpret_cast<const char*>(&node.lon), sizeof(node.lon));
        
        // Write tags
        if (!write_tag_vector(out, node.tags)) return false;
        
        if (!out.good()) return false;
    }
    
    // Serialize street segments
    const auto segment_count = db.get_segment_count();
    if (!write_vector_size(out, segment_count)) return false;
    
    for (std::size_t i = 0; i < segment_count; ++i) {
        const auto& seg = db.get_segment_by_index(i);
        
        // Write OSMID, category, max_speed_kph
        out.write(reinterpret_cast<const char*>(&seg.osm_id), sizeof(seg.osm_id));
        out.write(reinterpret_cast<const char*>(&seg.category), sizeof(seg.category));
        out.write(reinterpret_cast<const char*>(&seg.max_speed_kph), sizeof(seg.max_speed_kph));
        
        // Write name
        if (!write_string(out, seg.name)) return false;
        
        // Write node_refs
        if (!write_osmid_vector(out, seg.node_refs)) return false;
        
        // Write is_closed
        out.write(reinterpret_cast<const char*>(&seg.is_closed), sizeof(seg.is_closed));
        
        // Write tags
        if (!write_tag_vector(out, seg.tags)) return false;
        
        if (!out.good()) return false;
    }
    
    // Serialize POIs
    const auto poi_count = db.get_poi_count();
    if (!write_vector_size(out, poi_count)) return false;
    
    for (std::size_t i = 0; i < poi_count; ++i) {
        const auto pos = db.get_poi_position(i);
        const auto name = db.get_poi_name(i);
        const auto type = db.get_poi_type(i);
        
        // Write OSMID (we need to get this from the database)
        const auto osm_id = db.get_poi_osm_id(i);
        out.write(reinterpret_cast<const char*>(&osm_id), sizeof(osm_id));
        
        // Write lat, lon
        const double lat = pos.latitude();
        const double lon = pos.longitude();
        out.write(reinterpret_cast<const char*>(&lat), sizeof(lat));
        out.write(reinterpret_cast<const char*>(&lon), sizeof(lon));
        
        // Write category and name
        if (!write_string(out, type)) return false;
        if (!write_string(out, name)) return false;
        
        // Write empty tags for now (POI tags not yet implemented in BinaryDatabase)
        std::vector<std::pair<std::string, std::string>> empty_tags;
        if (!write_tag_vector(out, empty_tags)) return false;
        
        if (!out.good()) return false;
    }
    
    // Serialize features
    const auto feature_count = db.get_feature_count();
    if (!write_vector_size(out, feature_count)) return false;
    
    for (std::size_t i = 0; i < feature_count; ++i) {
        const auto osm_id = db.get_feature_osm_id(i);
        const auto type = db.get_feature_type(i);
        const auto name = db.get_feature_name(i);
        const auto point_count = db.get_feature_point_count(i);
        
        // Write OSMID, type
        out.write(reinterpret_cast<const char*>(&osm_id), sizeof(osm_id));
        const uint8_t type_byte = static_cast<uint8_t>(type);
        out.write(reinterpret_cast<const char*>(&type_byte), sizeof(type_byte));
        
        // Write name
        if (!write_string(out, name)) return false;
        
        // Write node_refs (we need to reconstruct this from point data)
        std::vector<OSMID> node_refs;
        node_refs.reserve(point_count);
        for (std::size_t j = 0; j < point_count; ++j) {
            // TODO: Find OSMID from lat/lon - for now use dummy value
            // const auto point = db.get_feature_point(j, i);
            node_refs.push_back(0);
        }
        if (!write_osmid_vector(out, node_refs)) return false;
        
        // Write is_closed (simplified - assume false for now)
        const bool is_closed = false;
        out.write(reinterpret_cast<const char*>(&is_closed), sizeof(is_closed));
        
        // Write empty tags for now
        std::vector<std::pair<std::string, std::string>> empty_tags;
        if (!write_tag_vector(out, empty_tags)) return false;
        
        if (!out.good()) return false;
    }
    
    // Serialize relations
    const auto relation_count = db.get_relation_count();
    if (!write_vector_size(out, relation_count)) return false;
    
    for (std::size_t i = 0; i < relation_count; ++i) {
        const auto& relation = db.get_relation_by_index(i);
        
        // Write OSMID
        out.write(reinterpret_cast<const char*>(&relation.osm_id), sizeof(relation.osm_id));
        
        // Write tags
        if (!write_tag_vector(out, relation.tags)) return false;
        
        // Write member_ids
        if (!write_osmid_vector(out, relation.member_ids)) return false;
        
        // Write member_types
        if (!write_vector_size(out, relation.member_types.size())) return false;
        if (!relation.member_types.empty()) {
            out.write(reinterpret_cast<const char*>(relation.member_types.data()), 
                     relation.member_types.size() * sizeof(std::uint8_t));
        }
        
        // Write member_roles
        if (!write_string_vector(out, relation.member_roles)) return false;
        
        if (!out.good()) return false;
    }
    
    // Serialize lookup maps
    const auto& node_map = db.get_node_id_to_index();
    const auto& way_map = db.get_way_id_to_segment_index();
    
    // Serialize node_id_to_index map
    if (!write_vector_size(out, node_map.size())) return false;
    for (const auto& pair : node_map) {
        out.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        out.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
    }
    
    // Serialize way_id_to_segment_index map
    if (!write_vector_size(out, way_map.size())) return false;
    for (const auto& pair : way_map) {
        out.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        out.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
    }
    
    // Serialize relation_id_to_index map (empty for now)
    const std::size_t relation_map_size = 0;
    if (!write_vector_size(out, relation_map_size)) return false;
    
    // Serialize street_name_to_id map (empty for now)
    const std::size_t street_map_size = 0;
    if (!write_vector_size(out, street_map_size)) return false;
    
    // Serialize intersection data
    const auto intersection_count = db.get_intersection_count();
    if (!write_vector_size(out, intersection_count)) return false;
    
    for (std::size_t i = 0; i < intersection_count; ++i) {
        const auto osm_id = db.get_intersection_osm_node_id(i);
        out.write(reinterpret_cast<const char*>(&osm_id), sizeof(osm_id));
        
        const auto seg_count = db.get_intersection_street_segment_count(i);
        const std::uint32_t seg_count_32 = static_cast<std::uint32_t>(seg_count);
        out.write(reinterpret_cast<const char*>(&seg_count_32), sizeof(seg_count_32));
        
        for (unsigned j = 0; j < seg_count; ++j) {
            const auto seg_idx = db.get_intersection_street_segment(j, i);
            out.write(reinterpret_cast<const char*>(&seg_idx), sizeof(seg_idx));
        }
        
        if (!out.good()) return false;
    }
    
    // Serialize R-tree spatial indexes
    try {
        db.get_street_rtree().serialize(out);
        db.get_intersection_rtree().serialize(out);
        db.get_poi_rtree().serialize(out);
        db.get_feature_rtree().serialize(out);
    } catch (const std::exception& ex) {
        std::cerr << "Error serializing R-trees: " << ex.what() << std::endl;
        return false;
    }
    
    return out.good();
}

bool CacheManager::deserialize_database(std::istream& in, BinaryDatabase& db, const CacheMetadata& metadata) const {
    // Clear existing data
    db.clear();
    
    // Set map bounds from metadata
    db.set_map_bounds(metadata.min_lat, metadata.max_lat, metadata.min_lon, metadata.max_lon, metadata.avg_lat_rad);
    
    // Deserialize nodes
    std::size_t node_count;
    if (!read_vector_size(in, node_count)) return false;
    
    for (std::size_t i = 0; i < node_count; ++i) {
        BinaryDatabase::Node node;
        
        // Read OSMID, lat, lon
        in.read(reinterpret_cast<char*>(&node.osm_id), sizeof(node.osm_id));
        in.read(reinterpret_cast<char*>(&node.lat), sizeof(node.lat));
        in.read(reinterpret_cast<char*>(&node.lon), sizeof(node.lon));
        
        // Read tags
        if (!read_tag_vector(in, node.tags)) return false;
        
        if (!in.good()) return false;
        
        db.add_node(node);
    }
    
    // Deserialize street segments
    std::size_t segment_count;
    if (!read_vector_size(in, segment_count)) return false;
    
    for (std::size_t i = 0; i < segment_count; ++i) {
        BinaryDatabase::StreetSegment seg;
        
        // Read OSMID, category, max_speed_kph
        in.read(reinterpret_cast<char*>(&seg.osm_id), sizeof(seg.osm_id));
        in.read(reinterpret_cast<char*>(&seg.category), sizeof(seg.category));
        in.read(reinterpret_cast<char*>(&seg.max_speed_kph), sizeof(seg.max_speed_kph));
        
        // Read name
        if (!read_string(in, seg.name)) return false;
        
        // Read node_refs
        if (!read_osmid_vector(in, seg.node_refs)) return false;
        
        // Read is_closed
        in.read(reinterpret_cast<char*>(&seg.is_closed), sizeof(seg.is_closed));
        
        // Read tags
        if (!read_tag_vector(in, seg.tags)) return false;
        
        if (!in.good()) return false;
        
        db.add_segment(seg);
    }
    
    // Deserialize POIs
    std::size_t poi_count;
    if (!read_vector_size(in, poi_count)) return false;
    
    for (std::size_t i = 0; i < poi_count; ++i) {
        OSMID osm_id;
        double lat, lon;
        std::string category, name;
        std::vector<std::pair<std::string, std::string>> tags;
        
        // Read OSMID, lat, lon
        in.read(reinterpret_cast<char*>(&osm_id), sizeof(osm_id));
        in.read(reinterpret_cast<char*>(&lat), sizeof(lat));
        in.read(reinterpret_cast<char*>(&lon), sizeof(lon));
        
        // Read category and name
        if (!read_string(in, category)) return false;
        if (!read_string(in, name)) return false;
        
        // Read tags
        if (!read_tag_vector(in, tags)) return false;
        
        if (!in.good()) return false;
        
        BinaryDatabase::POI poi;
        poi.osm_id = osm_id;
        poi.lat = lat;
        poi.lon = lon;
        poi.category = category;
        poi.name = name;
        poi.tags = tags;
        
        db.add_poi(poi);
    }
    
    // Deserialize features
    std::size_t feature_count;
    if (!read_vector_size(in, feature_count)) return false;
    
    for (std::size_t i = 0; i < feature_count; ++i) {
        OSMID osm_id;
        uint8_t type_byte;
        std::string name;
        std::vector<OSMID> node_refs;
        bool is_closed;
        std::vector<std::pair<std::string, std::string>> tags;
        
        // Read OSMID, type
        in.read(reinterpret_cast<char*>(&osm_id), sizeof(osm_id));
        in.read(reinterpret_cast<char*>(&type_byte), sizeof(type_byte));
        
        // Read name
        if (!read_string(in, name)) return false;
        
        // Read node_refs
        if (!read_osmid_vector(in, node_refs)) return false;
        
        // Read is_closed
        in.read(reinterpret_cast<char*>(&is_closed), sizeof(is_closed));
        
        // Read tags
        if (!read_tag_vector(in, tags)) return false;
        
        if (!in.good()) return false;
        
        BinaryDatabase::Feature feature;
        feature.osm_id = osm_id;
        feature.type = static_cast<FeatureType>(type_byte);
        feature.name = name;
        feature.node_refs = node_refs;
        feature.is_closed = is_closed;
        feature.tags = tags;
        
        db.add_feature(feature);
    }
    
    // Deserialize relations
    std::size_t relation_count;
    if (!read_vector_size(in, relation_count)) return false;
    
    for (std::size_t i = 0; i < relation_count; ++i) {
        BinaryDatabase::Relation relation;
        
        // Read OSMID
        in.read(reinterpret_cast<char*>(&relation.osm_id), sizeof(relation.osm_id));
        
        // Read tags
        if (!read_tag_vector(in, relation.tags)) return false;
        
        // Read member_ids
        if (!read_osmid_vector(in, relation.member_ids)) return false;
        
        // Read member_types
        std::size_t member_types_size;
        if (!read_vector_size(in, member_types_size)) return false;
        relation.member_types.resize(member_types_size);
        if (!relation.member_types.empty()) {
            in.read(reinterpret_cast<char*>(relation.member_types.data()), 
                   relation.member_types.size() * sizeof(std::uint8_t));
        }
        
        // Read member_roles
        if (!read_string_vector(in, relation.member_roles)) return false;
        
        if (!in.good()) return false;
        
        db.add_relation(relation);
    }
    
    // Deserialize lookup maps
    std::unordered_map<OSMID, std::size_t> node_map;
    std::unordered_map<OSMID, std::size_t> way_map;
    std::unordered_map<OSMID, std::size_t> relation_map;
    std::unordered_map<std::string, std::size_t> street_map;
    
    // Deserialize node_id_to_index map
    std::size_t node_map_size;
    if (!read_vector_size(in, node_map_size)) return false;
    for (std::size_t i = 0; i < node_map_size; ++i) {
        OSMID osm_id;
        std::size_t index;
        in.read(reinterpret_cast<char*>(&osm_id), sizeof(osm_id));
        in.read(reinterpret_cast<char*>(&index), sizeof(index));
        node_map[osm_id] = index;
    }
    
    // Deserialize way_id_to_segment_index map
    std::size_t way_map_size;
    if (!read_vector_size(in, way_map_size)) return false;
    for (std::size_t i = 0; i < way_map_size; ++i) {
        OSMID osm_id;
        std::size_t index;
        in.read(reinterpret_cast<char*>(&osm_id), sizeof(osm_id));
        in.read(reinterpret_cast<char*>(&index), sizeof(index));
        way_map[osm_id] = index;
    }
    
    // Deserialize relation_id_to_index map (empty for now)
    std::size_t relation_map_size;
    if (!read_vector_size(in, relation_map_size)) return false;
    
    // Deserialize street_name_to_id map (empty for now)
    std::size_t street_map_size;
    if (!read_vector_size(in, street_map_size)) return false;
    
    // Set lookup maps
    db.set_lookup_maps(node_map, way_map, relation_map, street_map);
    
    // Deserialize intersection data
    std::size_t intersection_count;
    if (!read_vector_size(in, intersection_count)) return false;
    
    std::vector<std::vector<std::size_t>> intersection_segments;
    std::vector<OSMID> intersection_node_ids;
    
    for (std::size_t i = 0; i < intersection_count; ++i) {
        OSMID osm_id;
        std::uint32_t seg_count_32;
        
        in.read(reinterpret_cast<char*>(&osm_id), sizeof(osm_id));
        in.read(reinterpret_cast<char*>(&seg_count_32), sizeof(seg_count_32));
        
        std::vector<std::size_t> segments;
        segments.reserve(seg_count_32);
        
        for (std::uint32_t j = 0; j < seg_count_32; ++j) {
            StreetSegmentIdx seg_idx;
            in.read(reinterpret_cast<char*>(&seg_idx), sizeof(seg_idx));
            segments.push_back(seg_idx);
        }
        
        if (!in.good()) return false;
        
        intersection_segments.push_back(segments);
        intersection_node_ids.push_back(osm_id);
    }
    
    // Set intersection data
    db.set_intersection_data(intersection_segments, intersection_node_ids);
    
    // Deserialize R-tree spatial indexes
    try {
        db.deserialize_street_rtree(in);
        db.deserialize_intersection_rtree(in);
        db.deserialize_poi_rtree(in);
        db.deserialize_feature_rtree(in);
    } catch (const std::exception& ex) {
        std::cerr << "Error deserializing R-trees: " << ex.what() << std::endl;
        // Fallback: rebuild spatial indexes from loaded data
        db.rebuild_spatial_indexes();
    }
    
    return in.good();
}

// Helper methods for serialization
bool CacheManager::write_vector_size(std::ostream& out, std::size_t size) const {
    const std::uint64_t size_64 = static_cast<std::uint64_t>(size);
    out.write(reinterpret_cast<const char*>(&size_64), sizeof(size_64));
    return out.good();
}

bool CacheManager::read_vector_size(std::istream& in, std::size_t& size) const {
    std::uint64_t size_64;
    in.read(reinterpret_cast<char*>(&size_64), sizeof(size_64));
    size = static_cast<std::size_t>(size_64);
    return in.good();
}

bool CacheManager::write_string_vector(std::ostream& out, const std::vector<std::string>& vec) const {
    if (!write_vector_size(out, vec.size())) return false;
    for (const auto& str : vec) {
        if (!write_string(out, str)) return false;
    }
    return out.good();
}

bool CacheManager::read_string_vector(std::istream& in, std::vector<std::string>& vec) const {
    std::size_t size;
    if (!read_vector_size(in, size)) return false;
    vec.resize(size);
    for (std::size_t i = 0; i < size; ++i) {
        if (!read_string(in, vec[i])) return false;
    }
    return in.good();
}

bool CacheManager::write_tag_vector(std::ostream& out, const std::vector<std::pair<std::string, std::string>>& tags) const {
    if (!write_vector_size(out, tags.size())) return false;
    for (const auto& tag : tags) {
        if (!write_string(out, tag.first)) return false;
        if (!write_string(out, tag.second)) return false;
    }
    return out.good();
}

bool CacheManager::read_tag_vector(std::istream& in, std::vector<std::pair<std::string, std::string>>& tags) const {
    std::size_t size;
    if (!read_vector_size(in, size)) return false;
    tags.resize(size);
    for (std::size_t i = 0; i < size; ++i) {
        if (!read_string(in, tags[i].first)) return false;
        if (!read_string(in, tags[i].second)) return false;
    }
    return in.good();
}

bool CacheManager::write_osmid_vector(std::ostream& out, const std::vector<OSMID>& vec) const {
    if (!write_vector_size(out, vec.size())) return false;
    if (!vec.empty()) {
        out.write(reinterpret_cast<const char*>(vec.data()), vec.size() * sizeof(OSMID));
    }
    return out.good();
}

bool CacheManager::read_osmid_vector(std::istream& in, std::vector<OSMID>& vec) const {
    std::size_t size;
    if (!read_vector_size(in, size)) return false;
    vec.resize(size);
    if (!vec.empty()) {
        in.read(reinterpret_cast<char*>(vec.data()), vec.size() * sizeof(OSMID));
    }
    return in.good();
}

std::string CacheManager::compute_file_checksum(const std::string& file_path) const {
    return compute_checksum_hex(file_path);
}

} // namespace gisevo
