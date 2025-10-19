#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
#include "StreetsDatabaseAPI.h"

namespace gisevo {

class BinaryDatabase;

class CacheManager {
public:
    struct CacheMetadata {
        static constexpr std::size_t kChecksumLength = 64;

        char streets_checksum[kChecksumLength + 1] = {};
        char osm_checksum[kChecksumLength + 1] = {};
        std::uint64_t creation_timestamp = 0;
        double min_lat = 0.0;
        double max_lat = 0.0;
        double min_lon = 0.0;
        double max_lon = 0.0;
        double avg_lat_rad = 0.0;
        std::uint32_t version = 0;

        std::string streets_checksum_string() const;
        std::string osm_checksum_string() const;
    };

    struct ValidationResult {
        bool valid = false;
        bool exists = false;
        std::string reason;
        CacheMetadata metadata;
        std::string streets_checksum;
        std::string osm_checksum;
    };

    CacheManager();

    ValidationResult validate_cache(const std::string& cache_path,
                                    const std::string& streets_path,
                                    const std::string& osm_path) const;

    bool load_cache(const std::string& cache_path, BinaryDatabase& db) const;
    bool save_cache(const std::string& cache_path,
                    const BinaryDatabase& db,
                    const std::string& streets_checksum,
                    const std::string& osm_checksum) const;

    CacheMetadata read_metadata(const std::string& cache_path) const;

    std::string compute_file_checksum(const std::string& file_path) const;

private:
    std::string compute_checksum_hex(const std::string& file_path) const;
    bool write_metadata(std::ostream& out, const CacheMetadata& metadata) const;
    bool read_metadata(std::istream& in, CacheMetadata& metadata) const;
    bool ensure_directory_exists(const std::string& cache_path) const;
    
    // Serialization/deserialization methods
    bool serialize_database(std::ostream& out, const BinaryDatabase& db) const;
    bool deserialize_database(std::istream& in, BinaryDatabase& db, const CacheMetadata& metadata) const;
    
    // Helper methods for serializing data structures
    bool write_vector_size(std::ostream& out, std::size_t size) const;
    bool read_vector_size(std::istream& in, std::size_t& size) const;
    bool write_string_vector(std::ostream& out, const std::vector<std::string>& vec) const;
    bool read_string_vector(std::istream& in, std::vector<std::string>& vec) const;
    bool write_tag_vector(std::ostream& out, const std::vector<std::pair<std::string, std::string>>& tags) const;
    bool read_tag_vector(std::istream& in, std::vector<std::pair<std::string, std::string>>& tags) const;
    bool write_osmid_vector(std::ostream& out, const std::vector<OSMID>& vec) const;
    bool read_osmid_vector(std::istream& in, std::vector<OSMID>& vec) const;

    static constexpr const char* kCacheMagic = "GISEVOC1";
    static constexpr std::uint32_t kCacheVersion = 1;
};

} // namespace gisevo
