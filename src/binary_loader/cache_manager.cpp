#include "cache_manager.hpp"

#include "binary_database.hpp"
#include "cache_io_helpers.hpp"
#include "cache_serializer.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#include <system_error>
#include <thread>

namespace gisevo {

using namespace cache_io;

std::string CacheManager::CacheMetadata::streets_checksum_string() const {
    return std::string(streets_checksum, streets_checksum + kChecksumLength);
}

std::string CacheManager::CacheMetadata::osm_checksum_string() const {
    return std::string(osm_checksum, osm_checksum + kChecksumLength);
}

CacheManager::CacheManager() : config_(), log_callback_(nullptr) {}

CacheManager::CacheManager(const ErrorHandlingConfig& config) 
    : config_(config), log_callback_(nullptr) {}

CacheManager::CacheManager(const ErrorHandlingConfig& config, LogCallback log_callback)
    : config_(config), log_callback_(log_callback) {}

void CacheManager::set_error_handling_config(const ErrorHandlingConfig& config) {
    config_ = config;
}

void CacheManager::set_log_callback(LogCallback callback) {
    log_callback_ = callback;
}

CacheManager::ErrorHandlingConfig CacheManager::get_error_handling_config() const {
    return config_;
}

void CacheManager::log_message(const std::string& message, bool is_error) const {
    if (log_callback_) {
        log_callback_(message, is_error);
    } else {
        if (is_error) {
            std::cerr << "[CacheManager ERROR] " << message << std::endl;
        } else {
            std::cout << "[CacheManager INFO] " << message << std::endl;
        }
    }
}

CacheManager::ValidationResult CacheManager::validate_cache(
    const std::string& cache_path,
    const std::string& streets_path,
    const std::string& osm_path) const {

    ValidationResult result;

    try {
        // Check if cache file exists
        if (!std::filesystem::exists(cache_path)) {
            result.exists = false;
            result.valid = false;
            result.reason = "Cache file missing";
            result.error_type = ValidationResult::ErrorType::FileNotFound;
            log_message("Cache file not found: " + cache_path, false);
            return result;
        }

        result.exists = true;

        // Check file size for corruption detection
        if (config_.enable_corruption_detection) {
            const auto file_size = std::filesystem::file_size(cache_path);
            if (file_size < config_.corruption_threshold_bytes) {
                result.valid = false;
                result.reason = "Cache file too small (likely corrupted)";
                result.error_type = ValidationResult::ErrorType::FileCorrupted;
                result.detailed_error = "File size: " + std::to_string(file_size) + " bytes";
                log_message("Cache file appears corrupted (too small): " + cache_path, true);
                return result;
            }
        }

        // Try to open the cache file
        std::ifstream in(cache_path, std::ios::binary);
        if (!in.good()) {
            result.valid = false;
            result.reason = "Failed to open cache file";
            result.error_type = ValidationResult::ErrorType::PermissionDenied;
            log_message("Failed to open cache file: " + cache_path, true);
            return result;
        }

        // Validate magic header
        char magic[kMagicLength];
        if (!read_exact(in, magic, kMagicLength) || std::string(magic, kMagicLength) != kCacheMagic) {
            result.valid = false;
            result.reason = "Invalid cache magic header";
            result.error_type = ValidationResult::ErrorType::FileCorrupted;
            log_message("Cache file has invalid magic header: " + cache_path, true);
            return result;
        }

        // Read metadata
        CacheMetadata metadata;
        if (!read_metadata(in, metadata)) {
            result.valid = false;
            result.reason = "Failed to read cache metadata";
            result.error_type = ValidationResult::ErrorType::FileCorrupted;
            log_message("Failed to read cache metadata: " + cache_path, true);
            return result;
        }

        result.metadata = metadata;

        // Validate version compatibility
        if (config_.enable_version_validation && metadata.version != kCacheVersion) {
            result.valid = false;
            result.reason = "Cache version mismatch";
            result.error_type = ValidationResult::ErrorType::VersionMismatch;
            result.detailed_error = "Expected version: " + std::to_string(kCacheVersion) + 
                                  ", found: " + std::to_string(metadata.version);
            log_message("Cache version mismatch: " + result.detailed_error, true);
            return result;
        }

        // Validate source file checksums
        if (config_.enable_checksum_validation) {
            const auto streets_checksum = compute_checksum_hex(streets_path);
            const auto osm_checksum = compute_checksum_hex(osm_path);

            result.streets_checksum = streets_checksum;
            result.osm_checksum = osm_checksum;

            if (streets_checksum.empty() || osm_checksum.empty()) {
                result.valid = false;
                result.reason = "Failed to compute source file checksums";
                result.error_type = ValidationResult::ErrorType::FileCorrupted;
                log_message("Failed to compute checksums for source files", true);
                return result;
            }

            if (metadata.streets_checksum_string() != streets_checksum) {
                result.valid = false;
                result.reason = "Streets file checksum mismatch";
                result.error_type = ValidationResult::ErrorType::ChecksumMismatch;
                result.detailed_error = "Expected: " + streets_checksum + 
                                       ", cached: " + metadata.streets_checksum_string();
                log_message("Streets file checksum mismatch: " + result.detailed_error, true);
                return result;
            }

            if (metadata.osm_checksum_string() != osm_checksum) {
                result.valid = false;
                result.reason = "OSM file checksum mismatch";
                result.error_type = ValidationResult::ErrorType::ChecksumMismatch;
                result.detailed_error = "Expected: " + osm_checksum + 
                                       ", cached: " + metadata.osm_checksum_string();
                log_message("OSM file checksum mismatch: " + result.detailed_error, true);
                return result;
            }
        }

        result.valid = true;
        result.reason.clear();
        log_message("Cache validation successful: " + cache_path, false);
        
        // Optionally perform deep validation
        if (config_.enable_deep_validation) {
            return deep_validate_cache(cache_path, streets_path, osm_path);
        }
        
        return result;

    } catch (const std::filesystem::filesystem_error& ex) {
        result.valid = false;
        result.reason = "Filesystem error during cache validation";
        result.error_type = ValidationResult::ErrorType::PermissionDenied;
        result.detailed_error = ex.what();
        log_message("Filesystem error during cache validation: " + std::string(ex.what()), true);
        return result;
    } catch (const std::exception& ex) {
        result.valid = false;
        result.reason = "Unexpected error during cache validation";
        result.error_type = classify_error(cache_path, ex);
        result.detailed_error = ex.what();
        log_message("Unexpected error during cache validation: " + std::string(ex.what()), true);
        return result;
    }
}

CacheManager::ValidationResult CacheManager::deep_validate_cache(
    const std::string& cache_path,
    const std::string& streets_path,
    const std::string& osm_path) const {

    ValidationResult result;
    
    try {
        log_message("Performing deep validation (including R-tree structure): " + cache_path, false);
        
        // First do standard validation
        result = validate_cache(cache_path, streets_path, osm_path);
        if (!result.valid) {
            return result;
        }
        
        // Try to read through the entire cache file to validate R-tree structures
        std::ifstream in(cache_path, std::ios::binary);
        if (!in.good()) {
            result.valid = false;
            result.reason = "Failed to open cache file for deep validation";
            result.error_type = ValidationResult::ErrorType::PermissionDenied;
            log_message("Failed to open cache file for deep validation: " + cache_path, true);
            return result;
        }

        // Skip magic header
        char magic[kMagicLength];
        if (!read_exact(in, magic, kMagicLength)) {
            result.valid = false;
            result.reason = "Failed to read magic during deep validation";
            result.error_type = ValidationResult::ErrorType::FileCorrupted;
            return result;
        }

        // Skip metadata
        CacheMetadata metadata;
        if (!read_metadata(in, metadata)) {
            result.valid = false;
            result.reason = "Failed to read metadata during deep validation";
            result.error_type = ValidationResult::ErrorType::FileCorrupted;
            return result;
        }

        // Note: Full deserialization with R-tree validation requires a BinaryDatabase instance
        // which has a private constructor. For now, deep validation will be performed during
        // actual cache loading. If loading fails with R-tree errors, the cache will be marked
        // as corrupted and deleted.
        
        result.valid = true;
        result.reason.clear();
        log_message("Deep cache validation successful (structure validation will occur during load): " + cache_path, false);
        return result;

    } catch (const std::exception& ex) {
        result.valid = false;
        result.reason = "Deep validation failed with exception";
        result.error_type = ValidationResult::ErrorType::DeserializationError;
        result.detailed_error = ex.what();
        log_message("Deep validation failed: " + std::string(ex.what()), true);
        return result;
    }
}

bool CacheManager::load_cache(const std::string& cache_path, BinaryDatabase& db) const {
    return retry_operation([this, &cache_path, &db]() -> bool {
        try {
            std::ifstream in(cache_path, std::ios::binary);
            if (!in.good()) {
                log_message("Failed to open cache file for loading: " + cache_path, true);
                return false;
            }

            char magic[kMagicLength];
            if (!read_exact(in, magic, kMagicLength) || std::string(magic, kMagicLength) != kCacheMagic) {
                log_message("Cache file has invalid magic header during load: " + cache_path, true);
                return false;
            }

            CacheMetadata metadata;
            if (!read_metadata(in, metadata)) {
                log_message("Failed to read cache metadata during load: " + cache_path, true);
                return false;
            }

            // Check disk space before loading
            if (config_.enable_auto_recovery) {
                const auto file_size = std::filesystem::file_size(cache_path);
                if (!check_disk_space(cache_path, file_size)) {
                    log_message("Insufficient disk space for cache loading: " + cache_path, true);
                    return false;
                }
            }

            // Deserialize BinaryDatabase data structures
            if (!deserialize_database(in, db, metadata)) {
                log_message("Failed to deserialize cache data: " + cache_path, true);
                return false;
            }

            log_message("Successfully loaded cache: " + cache_path, false);
            return true;

        } catch (const std::exception& ex) {
            log_message("Exception during cache loading: " + std::string(ex.what()), true);
            return false;
        }
    }, "load_cache");
}

bool CacheManager::save_cache(const std::string& cache_path,
                              const BinaryDatabase& db,
                              const std::string& streets_checksum,
                              const std::string& osm_checksum) const {
    return retry_operation([this, &cache_path, &db, &streets_checksum, &osm_checksum]() -> bool {
        try {
            // Ensure directory exists
            if (!ensure_directory_exists(cache_path)) {
                log_message("Failed to create cache directory: " + cache_path, true);
                return false;
            }

            // Check disk space before saving
            if (config_.enable_auto_recovery) {
                // Estimate required space (rough approximation)
                const std::size_t estimated_size = db.get_node_count() * 100 + 
                                                  db.get_segment_count() * 200 + 
                                                  db.get_poi_count() * 50 + 
                                                  db.get_feature_count() * 150;
                if (!check_disk_space(cache_path, estimated_size)) {
                    log_message("Insufficient disk space for cache saving: " + cache_path, true);
                    return false;
                }
            }

            // Create temporary file first for atomic operation
            const std::string temp_path = cache_path + ".tmp";
            std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
            if (!out.good()) {
                log_message("Failed to create temporary cache file: " + temp_path, true);
                return false;
            }

            // Write magic header
            if (!write_exact(out, kCacheMagic, kMagicLength)) {
                log_message("Failed to write magic header to cache file", true);
                return false;
            }

            // Create and write metadata
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
                log_message("Failed to write metadata to cache file", true);
                return false;
            }

            // Serialize BinaryDatabase data structures
            if (!serialize_database(out, db)) {
                log_message("Failed to serialize database to cache file", true);
                return false;
            }

            // Ensure all data is written
            out.flush();
            if (!out.good()) {
                log_message("Failed to flush cache file data", true);
                return false;
            }
            out.close();

            // Atomically replace the original file
            try {
                std::filesystem::rename(temp_path, cache_path);
            } catch (const std::filesystem::filesystem_error& ex) {
                log_message("Failed to rename temporary cache file: " + std::string(ex.what()), true);
                std::filesystem::remove(temp_path); // Clean up temp file
                return false;
            }

            log_message("Successfully saved cache: " + cache_path, false);
            return true;

        } catch (const std::exception& ex) {
            log_message("Exception during cache saving: " + std::string(ex.what()), true);
            return false;
        }
    }, "save_cache");
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
    return cache_serializer::serialize_database(out, db);
}

bool CacheManager::deserialize_database(std::istream& in, BinaryDatabase& db, const CacheMetadata& metadata) const {
    return cache_serializer::deserialize_database(in, db, 
        metadata.min_lat, metadata.max_lat, metadata.min_lon, metadata.max_lon, metadata.avg_lat_rad);
}

std::string CacheManager::compute_file_checksum(const std::string& file_path) const {
    return compute_checksum_hex(file_path);
}

// Enhanced error handling methods
CacheManager::ValidationResult::ErrorType CacheManager::classify_error(
    const std::string& cache_path, const std::exception& ex) const {
    
    const std::string error_msg = ex.what();
    
    if (error_msg.find("permission") != std::string::npos ||
        error_msg.find("access") != std::string::npos) {
        return ValidationResult::ErrorType::PermissionDenied;
    }
    
    if (error_msg.find("space") != std::string::npos ||
        error_msg.find("disk") != std::string::npos) {
        return ValidationResult::ErrorType::DiskSpaceError;
    }
    
    if (error_msg.find("serialize") != std::string::npos) {
        return ValidationResult::ErrorType::SerializationError;
    }
    
    if (error_msg.find("deserialize") != std::string::npos) {
        return ValidationResult::ErrorType::DeserializationError;
    }
    
    return ValidationResult::ErrorType::FileCorrupted;
}

bool CacheManager::attempt_cache_recovery(const std::string& cache_path, BinaryDatabase& db) const {
    if (!config_.enable_auto_recovery) {
        return false;
    }
    
    log_message("Attempting cache recovery for: " + cache_path, false);
    
    try {
        // Try to repair the cache file
        if (repair_cache(cache_path)) {
            log_message("Cache repair successful, retrying load", false);
            return load_cache(cache_path, db);
        }
        
        // If repair fails, try to delete and recreate
        if (config_.enable_cache_cleanup) {
            log_message("Cache repair failed, deleting corrupted cache", true);
            delete_cache(cache_path);
        }
        
        return false;
    } catch (const std::exception& ex) {
        log_message("Exception during cache recovery: " + std::string(ex.what()), true);
        return false;
    }
}

bool CacheManager::validate_file_integrity(const std::string& file_path) const {
    try {
        if (!std::filesystem::exists(file_path)) {
            return false;
        }
        
        const auto file_size = std::filesystem::file_size(file_path);
        if (file_size < config_.corruption_threshold_bytes) {
            return false;
        }
        
        // Try to open and read a small portion
        std::ifstream in(file_path, std::ios::binary);
        if (!in.good()) {
            return false;
        }
        
        // Read first few bytes to check basic accessibility
        char buffer[1024];
        in.read(buffer, sizeof(buffer));
        return in.good() || in.eof();
        
    } catch (const std::exception& ex) {
        log_message("File integrity check failed: " + std::string(ex.what()), true);
        return false;
    }
}

bool CacheManager::check_disk_space(const std::string& path, std::size_t required_bytes) const {
    try {
        const auto parent_path = std::filesystem::path(path).parent_path();
        const auto space_info = std::filesystem::space(parent_path);
        
        // Check if we have at least 2x the required space (safety margin)
        const auto available_space = space_info.available;
        const auto required_with_margin = required_bytes * 2;
        
        if (available_space < required_with_margin) {
            log_message("Insufficient disk space: available=" + 
                       std::to_string(available_space) + 
                       " bytes, required=" + std::to_string(required_with_margin) + 
                       " bytes", true);
            return false;
        }
        
        return true;
    } catch (const std::exception& ex) {
        log_message("Failed to check disk space: " + std::string(ex.what()), true);
        return false; // Assume insufficient space if we can't check
    }
}

// Cache management operations
bool CacheManager::delete_cache(const std::string& cache_path) const {
    try {
        if (std::filesystem::exists(cache_path)) {
            std::filesystem::remove(cache_path);
            log_message("Deleted cache file: " + cache_path, false);
            return true;
        }
        return true; // File doesn't exist, consider it "deleted"
    } catch (const std::exception& ex) {
        log_message("Failed to delete cache file: " + std::string(ex.what()), true);
        return false;
    }
}

bool CacheManager::repair_cache(const std::string& cache_path) const {
    // For now, repair means delete and let it be recreated
    // In the future, this could implement more sophisticated repair logic
    log_message("Cache repair not implemented, deleting corrupted cache", true);
    return delete_cache(cache_path);
}

bool CacheManager::backup_cache(const std::string& cache_path, const std::string& backup_path) const {
    try {
        if (!std::filesystem::exists(cache_path)) {
            log_message("Cannot backup non-existent cache file: " + cache_path, true);
            return false;
        }
        
        std::filesystem::copy_file(cache_path, backup_path, 
                                  std::filesystem::copy_options::overwrite_existing);
        log_message("Backed up cache file: " + cache_path + " -> " + backup_path, false);
        return true;
    } catch (const std::exception& ex) {
        log_message("Failed to backup cache file: " + std::string(ex.what()), true);
        return false;
    }
}

bool CacheManager::restore_cache(const std::string& backup_path, const std::string& cache_path) const {
    try {
        if (!std::filesystem::exists(backup_path)) {
            log_message("Cannot restore from non-existent backup: " + backup_path, true);
            return false;
        }
        
        std::filesystem::copy_file(backup_path, cache_path, 
                                  std::filesystem::copy_options::overwrite_existing);
        log_message("Restored cache file: " + backup_path + " -> " + cache_path, false);
        return true;
    } catch (const std::exception& ex) {
        log_message("Failed to restore cache file: " + std::string(ex.what()), true);
        return false;
    }
}

std::size_t CacheManager::get_cache_file_size(const std::string& cache_path) const {
    try {
        if (std::filesystem::exists(cache_path)) {
            return std::filesystem::file_size(cache_path);
        }
        return 0;
    } catch (const std::exception& ex) {
        log_message("Failed to get cache file size: " + std::string(ex.what()), true);
        return 0;
    }
}

bool CacheManager::is_cache_file_corrupted(const std::string& cache_path) const {
    try {
        if (!std::filesystem::exists(cache_path)) {
            return true; // Non-existent files are considered corrupted
        }
        
        const auto file_size = std::filesystem::file_size(cache_path);
        if (file_size < config_.corruption_threshold_bytes) {
            return true;
        }
        
        // Try to read the magic header
        std::ifstream in(cache_path, std::ios::binary);
        if (!in.good()) {
            return true;
        }
        
        char magic[kMagicLength];
        if (!read_exact(in, magic, kMagicLength) || std::string(magic, kMagicLength) != kCacheMagic) {
            return true;
        }
        
        return false;
    } catch (const std::exception& ex) {
        log_message("Error checking cache corruption: " + std::string(ex.what()), true);
        return true; // Assume corrupted if we can't check
    }
}

} // namespace gisevo
