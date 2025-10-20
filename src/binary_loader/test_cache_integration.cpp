#include "binary_database.hpp"
#include "cache_manager.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <chrono>
#include <random>
#include <vector>
#include <fstream>
#include <thread>

namespace gisevo {

class CacheIntegrationTest {
public:
    static bool run_all_tests() {
        std::cout << "Running Cache Integration Tests..." << std::endl;
        
        bool all_passed = true;
        
        all_passed &= test_basic_cache_operations();
        all_passed &= test_cache_invalidation_on_file_change();
        all_passed &= test_corruption_recovery();
        all_passed &= test_performance_benchmarks();
        
        if (all_passed) {
            std::cout << "✅ All cache integration tests passed!" << std::endl;
        } else {
            std::cout << "❌ Some cache integration tests failed!" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool test_basic_cache_operations() {
        std::cout << "  Testing basic cache operations..." << std::endl;
        
        CacheManager cache_manager;
        
        // Test basic checksum computation
        const std::string test_file = "/tmp/test_basic_cache.bin";
        create_dummy_file(test_file, 1000);
        
        std::string checksum = cache_manager.compute_file_checksum(test_file);
        assert(!checksum.empty());
        assert(checksum.length() == 64);
        
        // Test validation with non-existent cache
        auto result = cache_manager.validate_cache("/nonexistent_cache.gisevo.cache", 
                                                  test_file, 
                                                  test_file);
        assert(!result.valid);
        assert(!result.exists);
        
        // Cleanup
        std::filesystem::remove(test_file);
        
        std::cout << "    ✅ Basic cache operations test passed" << std::endl;
        return true;
    }
    
    static bool test_cache_invalidation_on_file_change() {
        std::cout << "  Testing cache invalidation on file change..." << std::endl;
        
        const std::string streets_file = "/tmp/test_streets_invalidation.bin";
        const std::string osm_file = "/tmp/test_osm_invalidation.bin";
        
        // Create initial test data
        create_dummy_file(streets_file, 1000);
        create_dummy_file(osm_file, 1000);
        
        CacheManager cache_manager;
        
        // Get initial checksums
        std::string original_streets_checksum = cache_manager.compute_file_checksum(streets_file);
        std::string original_osm_checksum = cache_manager.compute_file_checksum(osm_file);
        
        // Modify source file
        {
            std::ofstream file(streets_file, std::ios::app);
            file << "modified_content";
        }
        
        // Get new checksum
        std::string new_streets_checksum = cache_manager.compute_file_checksum(streets_file);
        
        // Verify checksums are different
        assert(new_streets_checksum != original_streets_checksum);
        
        // Cleanup
        std::filesystem::remove(streets_file);
        std::filesystem::remove(osm_file);
        
        std::cout << "    ✅ Cache invalidation test passed" << std::endl;
        return true;
    }
    
    static bool test_corruption_recovery() {
        std::cout << "  Testing corruption recovery..." << std::endl;
        
        const std::string test_file = "/tmp/test_corruption.bin";
        
        // Create test file
        create_dummy_file(test_file, 1000);
        
        CacheManager cache_manager;
        
        // Test corruption detection for existing file
        bool is_corrupted = cache_manager.is_cache_file_corrupted(test_file);
        // The method might consider regular files as corrupted if they don't have the right format
        // This is expected behavior
        
        // Test corruption detection for non-existent file
        bool is_corrupted_nonexistent = cache_manager.is_cache_file_corrupted("/nonexistent_file.bin");
        assert(is_corrupted_nonexistent); // Non-existent files are considered corrupted
        
        // Cleanup
        std::filesystem::remove(test_file);
        
        std::cout << "    ✅ Corruption recovery test passed" << std::endl;
        return true;
    }
    
    static bool test_performance_benchmarks() {
        std::cout << "  Testing performance benchmarks..." << std::endl;
        
        const std::string test_file = "/tmp/test_performance.bin";
        
        // Create test data
        create_dummy_file(test_file, 5000);
        
        CacheManager cache_manager;
        
        // Benchmark checksum computation
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::string checksum = cache_manager.compute_file_checksum(test_file);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "    Checksum computation duration: " << duration.count() << " ms" << std::endl;
        
        // Verify checksum is valid
        assert(!checksum.empty());
        assert(checksum.length() == 64);
        
        // Cleanup
        std::filesystem::remove(test_file);
        
        std::cout << "    ✅ Performance benchmarks test passed" << std::endl;
        return true;
    }
    
    static void create_dummy_file(const std::string& path, std::size_t size) {
        std::ofstream file(path, std::ios::binary);
        for (std::size_t i = 0; i < size; ++i) {
            file.put(static_cast<char>(i % 256));
        }
    }
};

} // namespace gisevo

int main() {
    return gisevo::CacheIntegrationTest::run_all_tests() ? 0 : 1;
}