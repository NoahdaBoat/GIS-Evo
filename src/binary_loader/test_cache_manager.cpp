#include "cache_manager.hpp"
#include "binary_database.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <chrono>
#include <thread>

namespace gisevo {

class CacheManagerTest {
public:
    static bool run_all_tests() {
        std::cout << "Running CacheManager Unit Tests..." << std::endl;
        
        bool all_passed = true;
        
        all_passed &= test_checksum_computation();
        all_passed &= test_basic_validation();
        all_passed &= test_error_classification();
        all_passed &= test_retry_mechanism();
        
        if (all_passed) {
            std::cout << "✅ All CacheManager unit tests passed!" << std::endl;
        } else {
            std::cout << "❌ Some CacheManager unit tests failed!" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool test_checksum_computation() {
        std::cout << "  Testing checksum computation..." << std::endl;
        
        CacheManager cache_manager;
        
        // Create test files with known content
        const std::string test_file1 = "/tmp/test_checksum1.txt";
        const std::string test_file2 = "/tmp/test_checksum2.txt";
        
        // Create files with different content
        {
            std::ofstream file(test_file1);
            file << "Hello, World!";
        }
        
        {
            std::ofstream file(test_file2);
            file << "Hello, World!";
        }
        
        // Compute checksums
        std::string checksum1 = cache_manager.compute_file_checksum(test_file1);
        std::string checksum2 = cache_manager.compute_file_checksum(test_file2);
        
        // Same content should produce same checksum
        assert(checksum1 == checksum2);
        assert(!checksum1.empty());
        assert(checksum1.length() == 64); // SHA256 hex length
        
        // Modify one file
        {
            std::ofstream file(test_file2);
            file << "Hello, World! Modified";
        }
        
        std::string checksum3 = cache_manager.compute_file_checksum(test_file2);
        
        // Different content should produce different checksum
        assert(checksum1 != checksum3);
        
        // Cleanup
        std::filesystem::remove(test_file1);
        std::filesystem::remove(test_file2);
        
        std::cout << "    ✅ Checksum computation test passed" << std::endl;
        return true;
    }
    
    static bool test_basic_validation() {
        std::cout << "  Testing basic validation..." << std::endl;
        
        CacheManager cache_manager;
        
        // Test validation with non-existent files
        auto result = cache_manager.validate_cache("/nonexistent_cache.gisevo.cache", 
                                                  "/nonexistent_streets.bin", 
                                                  "/nonexistent_osm.bin");
        
        // Should fail because cache doesn't exist
        assert(!result.valid);
        assert(!result.exists);
        assert(result.error_type == CacheManager::ValidationResult::ErrorType::FileNotFound);
        
        std::cout << "    ✅ Basic validation test passed" << std::endl;
        return true;
    }
    
    static bool test_error_classification() {
        std::cout << "  Testing error classification..." << std::endl;
        
        CacheManager cache_manager;
        
        // Test error classification with a generic exception
        try {
            throw std::runtime_error("Test error");
        } catch (const std::exception& ex) {
            auto error_type = cache_manager.classify_error("/test/path", ex);
            // The classify_error method should handle generic exceptions
            assert(error_type != CacheManager::ValidationResult::ErrorType::NoError);
        }
        
        std::cout << "    ✅ Error classification test passed" << std::endl;
        return true;
    }
    
    static bool test_retry_mechanism() {
        std::cout << "  Testing retry mechanism..." << std::endl;
        
        CacheManager::ErrorHandlingConfig config;
        config.max_retry_attempts = 3;
        CacheManager cache_manager(config);
        
        int attempt_count = 0;
        
        // Test successful retry
        bool success = cache_manager.retry_operation([&attempt_count]() {
            attempt_count++;
            return attempt_count >= 2; // Succeed on second attempt
        }, "test_operation");
        
        assert(success);
        assert(attempt_count == 2);
        
        // Test failed retry
        attempt_count = 0;
        bool failure = cache_manager.retry_operation([&attempt_count]() {
            attempt_count++;
            return false; // Always fail
        }, "test_operation");
        
        assert(!failure);
        assert(attempt_count == 3);
        
        std::cout << "    ✅ Retry mechanism test passed" << std::endl;
        return true;
    }
};

} // namespace gisevo

int main() {
    return gisevo::CacheManagerTest::run_all_tests() ? 0 : 1;
}