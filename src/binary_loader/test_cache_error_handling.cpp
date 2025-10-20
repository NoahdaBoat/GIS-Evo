#include "cache_manager.hpp"
#include "binary_database.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

namespace gisevo {

class CacheErrorHandlingTest {
public:
    static bool run_all_tests() {
        std::cout << "Running Cache Error Handling Tests..." << std::endl;
        
        bool all_passed = true;
        
        all_passed &= test_corrupted_cache_detection();
        all_passed &= test_missing_cache_handling();
        all_passed &= test_version_mismatch_handling();
        all_passed &= test_checksum_mismatch_handling();
        all_passed &= test_permission_error_handling();
        all_passed &= test_disk_space_error_handling();
        all_passed &= test_retry_mechanism();
        all_passed &= test_cache_cleanup();
        all_passed &= test_backup_restore();
        
        if (all_passed) {
            std::cout << "✅ All cache error handling tests passed!" << std::endl;
        } else {
            std::cout << "❌ Some cache error handling tests failed!" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool test_corrupted_cache_detection() {
        std::cout << "  Testing corrupted cache detection..." << std::endl;
        
        // Create a corrupted cache file
        const std::string corrupted_cache = "/tmp/test_corrupted_cache.gisevo.cache";
        const std::string streets_file = "/tmp/test_streets.bin";
        const std::string osm_file = "/tmp/test_osm.bin";
        
        // Create dummy source files
        create_dummy_file(streets_file, 1000);
        create_dummy_file(osm_file, 1000);
        
        // Create corrupted cache file (too small)
        create_dummy_file(corrupted_cache, 100);
        
        CacheManager cache_manager;
        const auto validation = cache_manager.validate_cache(corrupted_cache, streets_file, osm_file);
        
        bool result = !validation.valid && 
                     validation.error_type == CacheManager::ValidationResult::ErrorType::FileCorrupted;
        
        // Cleanup
        std::filesystem::remove(corrupted_cache);
        std::filesystem::remove(streets_file);
        std::filesystem::remove(osm_file);
        
        std::cout << "    " << (result ? "✅" : "❌") << " Corrupted cache detection" << std::endl;
        return result;
    }
    
    static bool test_missing_cache_handling() {
        std::cout << "  Testing missing cache handling..." << std::endl;
        
        const std::string missing_cache = "/tmp/nonexistent_cache.gisevo.cache";
        const std::string streets_file = "/tmp/test_streets.bin";
        const std::string osm_file = "/tmp/test_osm.bin";
        
        // Create dummy source files
        create_dummy_file(streets_file, 1000);
        create_dummy_file(osm_file, 1000);
        
        CacheManager cache_manager;
        const auto validation = cache_manager.validate_cache(missing_cache, streets_file, osm_file);
        
        bool result = !validation.exists && 
                     validation.error_type == CacheManager::ValidationResult::ErrorType::FileNotFound;
        
        // Cleanup
        std::filesystem::remove(streets_file);
        std::filesystem::remove(osm_file);
        
        std::cout << "    " << (result ? "✅" : "❌") << " Missing cache handling" << std::endl;
        return result;
    }
    
    static bool test_version_mismatch_handling() {
        std::cout << "  Testing version mismatch handling..." << std::endl;
        
        const std::string cache_file = "/tmp/test_version_cache.gisevo.cache";
        const std::string streets_file = "/tmp/test_streets.bin";
        const std::string osm_file = "/tmp/test_osm.bin";
        
        // Create dummy source files
        create_dummy_file(streets_file, 1000);
        create_dummy_file(osm_file, 1000);
        
        // Create cache file with wrong version
        create_cache_with_wrong_version(cache_file);
        
        CacheManager cache_manager;
        const auto validation = cache_manager.validate_cache(cache_file, streets_file, osm_file);
        
        bool result = !validation.valid && 
                     validation.error_type == CacheManager::ValidationResult::ErrorType::VersionMismatch;
        
        // Cleanup
        std::filesystem::remove(cache_file);
        std::filesystem::remove(streets_file);
        std::filesystem::remove(osm_file);
        
        std::cout << "    " << (result ? "✅" : "❌") << " Version mismatch handling" << std::endl;
        return result;
    }
    
    static bool test_checksum_mismatch_handling() {
        std::cout << "  Testing checksum mismatch handling..." << std::endl;
        
        const std::string cache_file = "/tmp/test_checksum_cache.gisevo.cache";
        const std::string streets_file = "/tmp/test_streets.bin";
        const std::string osm_file = "/tmp/test_osm.bin";
        
        // Create dummy source files
        create_dummy_file(streets_file, 1000);
        create_dummy_file(osm_file, 1000);
        
        // Create cache file with different checksums
        create_cache_with_different_checksums(cache_file, streets_file, osm_file);
        
        CacheManager cache_manager;
        const auto validation = cache_manager.validate_cache(cache_file, streets_file, osm_file);
        
        bool result = !validation.valid && 
                     validation.error_type == CacheManager::ValidationResult::ErrorType::ChecksumMismatch;
        
        // Cleanup
        std::filesystem::remove(cache_file);
        std::filesystem::remove(streets_file);
        std::filesystem::remove(osm_file);
        
        std::cout << "    " << (result ? "✅" : "❌") << " Checksum mismatch handling" << std::endl;
        return result;
    }
    
    static bool test_permission_error_handling() {
        std::cout << "  Testing permission error handling..." << std::endl;
        
        // This test is platform-specific and may not work on all systems
        // For now, we'll test the error classification logic
        CacheManager cache_manager;
        
        try {
            throw std::runtime_error("permission denied");
        } catch (const std::exception& ex) {
            auto error_type = cache_manager.classify_error("/tmp/test", ex);
            bool result = error_type == CacheManager::ValidationResult::ErrorType::PermissionDenied;
            std::cout << "    " << (result ? "✅" : "❌") << " Permission error classification" << std::endl;
            return result;
        }
    }
    
    static bool test_disk_space_error_handling() {
        std::cout << "  Testing disk space error handling..." << std::endl;
        
        CacheManager cache_manager;
        
        try {
            throw std::runtime_error("no space left on device");
        } catch (const std::exception& ex) {
            auto error_type = cache_manager.classify_error("/tmp/test", ex);
            bool result = error_type == CacheManager::ValidationResult::ErrorType::DiskSpaceError;
            std::cout << "    " << (result ? "✅" : "❌") << " Disk space error classification" << std::endl;
            return result;
        }
    }
    
    static bool test_retry_mechanism() {
        std::cout << "  Testing retry mechanism..." << std::endl;
        
        CacheManager::ErrorHandlingConfig config;
        config.max_retry_attempts = 2;
        CacheManager cache_manager(config);
        
        int attempt_count = 0;
        auto retry_operation = [&attempt_count]() -> bool {
            attempt_count++;
            return attempt_count >= 2; // Succeed on second attempt
        };
        
        bool result = cache_manager.retry_operation(retry_operation, "test_retry");
        
        std::cout << "    " << (result ? "✅" : "❌") << " Retry mechanism" << std::endl;
        return result;
    }
    
    static bool test_cache_cleanup() {
        std::cout << "  Testing cache cleanup..." << std::endl;
        
        const std::string cache_file = "/tmp/test_cleanup_cache.gisevo.cache";
        
        // Create a dummy cache file
        create_dummy_file(cache_file, 1000);
        
        CacheManager cache_manager;
        bool result = cache_manager.delete_cache(cache_file);
        
        // Verify file is deleted
        result &= !std::filesystem::exists(cache_file);
        
        std::cout << "    " << (result ? "✅" : "❌") << " Cache cleanup" << std::endl;
        return result;
    }
    
    static bool test_backup_restore() {
        std::cout << "  Testing backup/restore..." << std::endl;
        
        const std::string cache_file = "/tmp/test_backup_cache.gisevo.cache";
        const std::string backup_file = "/tmp/test_backup_cache.gisevo.cache.bak";
        
        // Create a dummy cache file
        create_dummy_file(cache_file, 1000);
        
        CacheManager cache_manager;
        
        // Test backup
        bool backup_result = cache_manager.backup_cache(cache_file, backup_file);
        
        // Test restore
        std::filesystem::remove(cache_file); // Delete original
        bool restore_result = cache_manager.restore_cache(backup_file, cache_file);
        
        // Verify restore worked
        bool verify_result = std::filesystem::exists(cache_file);
        
        bool result = backup_result && restore_result && verify_result;
        
        // Cleanup
        std::filesystem::remove(cache_file);
        std::filesystem::remove(backup_file);
        
        std::cout << "    " << (result ? "✅" : "❌") << " Backup/restore" << std::endl;
        return result;
    }
    
    static void create_dummy_file(const std::string& path, std::size_t size) {
        std::ofstream out(path, std::ios::binary);
        for (std::size_t i = 0; i < size; ++i) {
            out.put(static_cast<char>(i % 256));
        }
    }
    
    static void create_cache_with_wrong_version(const std::string& path) {
        std::ofstream out(path, std::ios::binary);
        
        // Write magic header
        out.write("GISEVOC1", 8);
        
        // Write wrong version (999 instead of 1)
        uint32_t wrong_version = 999;
        out.write(reinterpret_cast<const char*>(&wrong_version), sizeof(wrong_version));
        
        // Write dummy metadata in correct format
        uint64_t timestamp = 0;
        double bounds[4] = {0.0, 0.0, 0.0, 0.0};
        out.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        out.write(reinterpret_cast<const char*>(bounds), sizeof(bounds));
        
        // Write checksums using write_string format (length + data)
        std::string dummy_checksum = "dummy_checksum_64_chars_long_string_that_should_be_exactly_64_chars";
        dummy_checksum.resize(64, '0');
        
        // Write first checksum
        uint32_t length = static_cast<uint32_t>(dummy_checksum.size());
        out.write(reinterpret_cast<const char*>(&length), sizeof(length));
        out.write(dummy_checksum.data(), length);
        
        // Write second checksum
        out.write(reinterpret_cast<const char*>(&length), sizeof(length));
        out.write(dummy_checksum.data(), length);
        
        // Add enough padding to exceed corruption threshold
        std::vector<char> padding(2000, 'X');
        out.write(padding.data(), padding.size());
    }
    
    static void create_cache_with_different_checksums(const std::string& cache_path,
                                                     const std::string& streets_path,
                                                     const std::string& osm_path) {
        std::ofstream out(cache_path, std::ios::binary);
        
        // Write magic header
        out.write("GISEVOC1", 8);
        
        // Write correct version
        uint32_t version = 1;
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        
        // Write dummy metadata in correct format
        uint64_t timestamp = 0;
        double bounds[4] = {0.0, 0.0, 0.0, 0.0};
        out.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        out.write(reinterpret_cast<const char*>(bounds), sizeof(bounds));
        
        // Write different checksums using write_string format
        std::string different_checksum = "different_checksum_64_chars_long_string_that_should_be_exactly_64_chars";
        different_checksum.resize(64, '0');
        
        // Write first checksum
        uint32_t length = static_cast<uint32_t>(different_checksum.size());
        out.write(reinterpret_cast<const char*>(&length), sizeof(length));
        out.write(different_checksum.data(), length);
        
        // Write second checksum
        out.write(reinterpret_cast<const char*>(&length), sizeof(length));
        out.write(different_checksum.data(), length);
        
        // Add enough padding to exceed corruption threshold
        std::vector<char> padding(2000, 'Y');
        out.write(padding.data(), padding.size());
    }
};

} // namespace gisevo

int main() {
    return gisevo::CacheErrorHandlingTest::run_all_tests() ? 0 : 1;
}
