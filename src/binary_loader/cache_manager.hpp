#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <thread>
#include "StreetsDatabaseAPI.h"

namespace gisevo {

class BinaryDatabase;

class CacheManager {
public:
    // Logging callback type for error reporting
    using LogCallback = std::function<void(const std::string& message, bool is_error)>;
    
    // Error handling configuration
    struct ErrorHandlingConfig {
        bool enable_auto_recovery = true;           // Automatically recover from cache errors
        bool enable_corruption_detection = true;    // Detect and handle corrupted cache files
        bool enable_version_validation = true;      // Validate cache version compatibility
        bool enable_checksum_validation = true;     // Validate source file checksums
        bool enable_fallback_loading = true;       // Fall back to binary loading on cache failure
        bool enable_cache_cleanup = true;          // Clean up invalid cache files
        bool enable_deep_validation = false;       // Perform deep validation including R-tree structure checks
        int max_retry_attempts = 3;                // Maximum retry attempts for cache operations
        std::size_t corruption_threshold_bytes = 1024; // Minimum file size to consider valid
    };

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
        
        // Additional error details
        enum class ErrorType {
            NoError,
            FileNotFound,
            FileCorrupted,
            VersionMismatch,
            ChecksumMismatch,
            PermissionDenied,
            DiskSpaceError,
            SerializationError,
            DeserializationError
        };
        ErrorType error_type = ErrorType::NoError;
        std::string detailed_error;
    };

    CacheManager();
    explicit CacheManager(const ErrorHandlingConfig& config);
    CacheManager(const ErrorHandlingConfig& config, LogCallback log_callback);

    // Configuration methods
    void set_error_handling_config(const ErrorHandlingConfig& config);
    void set_log_callback(LogCallback callback);
    ErrorHandlingConfig get_error_handling_config() const;

    // Enhanced validation with detailed error reporting
    ValidationResult validate_cache(const std::string& cache_path,
                                    const std::string& streets_path,
                                    const std::string& osm_path) const;
    
    // Deep validation that checks R-tree structure integrity
    ValidationResult deep_validate_cache(const std::string& cache_path,
                                         const std::string& streets_path,
                                         const std::string& osm_path) const;

    // Enhanced cache operations with error handling
    bool load_cache(const std::string& cache_path, BinaryDatabase& db) const;
    bool save_cache(const std::string& cache_path,
                    const BinaryDatabase& db,
                    const std::string& streets_checksum,
                    const std::string& osm_checksum) const;

    // Cache management operations
    bool delete_cache(const std::string& cache_path) const;
    bool repair_cache(const std::string& cache_path) const;
    bool backup_cache(const std::string& cache_path, const std::string& backup_path) const;
    bool restore_cache(const std::string& backup_path, const std::string& cache_path) const;

    // Utility methods
    CacheMetadata read_metadata(const std::string& cache_path) const;
    std::string compute_file_checksum(const std::string& file_path) const;
    std::size_t get_cache_file_size(const std::string& cache_path) const;
    bool is_cache_file_corrupted(const std::string& cache_path) const;

    // Testing and debugging methods
    ValidationResult::ErrorType classify_error(const std::string& cache_path, const std::exception& ex) const;
    bool attempt_cache_recovery(const std::string& cache_path, BinaryDatabase& db) const;
    
    template<typename Func>
    bool retry_operation(Func operation, const std::string& operation_name) const {
        int attempts = 0;
        const int max_attempts = config_.max_retry_attempts;
        
        while (attempts < max_attempts) {
            try {
                if (operation()) {
                    return true;
                }
            } catch (const std::exception& ex) {
                log_message("Exception in " + operation_name + " (attempt " + 
                           std::to_string(attempts + 1) + "): " + std::string(ex.what()), true);
            }
            
            attempts++;
            if (attempts < max_attempts) {
                // Exponential backoff with jitter
                const int base_delay_ms = 100;
                const int delay_ms = base_delay_ms * (1 << attempts) + (attempts * 50);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
        }
        
        log_message("Operation " + operation_name + " failed after " + 
                   std::to_string(max_attempts) + " attempts", true);
        return false;
    }

private:
    ErrorHandlingConfig config_;
    LogCallback log_callback_;
    
    // Core functionality
    std::string compute_checksum_hex(const std::string& file_path) const;
    bool write_metadata(std::ostream& out, const CacheMetadata& metadata) const;
    bool read_metadata(std::istream& in, CacheMetadata& metadata) const;
    bool ensure_directory_exists(const std::string& cache_path) const;
    
    // Enhanced error handling methods
    void log_message(const std::string& message, bool is_error = false) const;
    bool validate_file_integrity(const std::string& file_path) const;
    bool check_disk_space(const std::string& path, std::size_t required_bytes) const;
    
    // Serialization/deserialization methods
    bool serialize_database(std::ostream& out, const BinaryDatabase& db) const;
    bool deserialize_database(std::istream& in, BinaryDatabase& db, const CacheMetadata& metadata) const;

    static constexpr const char* kCacheMagic = "GISEVOC1";
    static constexpr std::uint32_t kCacheVersion = 1;
};

} // namespace gisevo
