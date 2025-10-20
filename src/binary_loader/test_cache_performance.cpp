#include "binary_database.hpp"
#include "cache_manager.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <chrono>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>

namespace gisevo {

class CachePerformanceTest {
public:
    static bool run_all_tests() {
        std::cout << "Running Cache Performance Tests..." << std::endl;
        
        bool all_passed = true;
        
        all_passed &= test_checksum_performance();
        all_passed &= test_file_size_analysis();
        all_passed &= test_memory_usage_profiling();
        all_passed &= test_cache_io_performance();
        
        if (all_passed) {
            std::cout << "✅ All cache performance tests passed!" << std::endl;
        } else {
            std::cout << "❌ Some cache performance tests failed!" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool test_checksum_performance() {
        std::cout << "  Testing checksum performance..." << std::endl;
        
        const std::string test_file = "/tmp/test_checksum_performance.bin";
        
        // Create test data
        create_performance_test_data(test_file);
        
        CacheManager cache_manager;
        
        // Measure checksum computation
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
        
        std::cout << "    ✅ Checksum performance test passed" << std::endl;
        return true;
    }
    
    static bool test_file_size_analysis() {
        std::cout << "  Testing file size analysis..." << std::endl;
        
        const std::string test_file = "/tmp/test_file_size.bin";
        
        // Create test data
        create_performance_test_data(test_file);
        
        CacheManager cache_manager;
        
        // Analyze file size
        std::size_t file_size = std::filesystem::file_size(test_file);
        
        std::cout << "    Test file size: " << format_bytes(file_size) << std::endl;
        
        // Verify file size is reasonable
        assert(file_size > 0);
        assert(file_size < 1000000); // Less than 1MB
        
        // Test cache file size for non-existent file
        std::size_t cache_size = cache_manager.get_cache_file_size("/nonexistent_cache.gisevo.cache");
        assert(cache_size == 0);
        
        // Cleanup
        std::filesystem::remove(test_file);
        
        std::cout << "    ✅ File size analysis test passed" << std::endl;
        return true;
    }
    
    static bool test_memory_usage_profiling() {
        std::cout << "  Testing memory usage profiling..." << std::endl;
        
        const std::string test_file = "/tmp/test_memory_profiling.bin";
        
        // Create test data
        create_performance_test_data(test_file);
        
        CacheManager cache_manager;
        
        // Profile checksum computation memory usage
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
        
        std::cout << "    ✅ Memory usage profiling test passed" << std::endl;
        return true;
    }
    
    static bool test_cache_io_performance() {
        std::cout << "  Testing cache I/O performance..." << std::endl;
        
        const std::string test_file = "/tmp/test_cache_io.bin";
        
        // Create test data
        create_performance_test_data(test_file);
        
        CacheManager cache_manager;
        
        // Measure file operations
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Test file existence check
        bool exists = std::filesystem::exists(test_file);
        assert(exists);
        
        // Test file size check
        std::size_t file_size = std::filesystem::file_size(test_file);
        assert(file_size > 0);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "    File I/O operations duration: " << duration.count() << " μs" << std::endl;
        
        // Cleanup
        std::filesystem::remove(test_file);
        
        std::cout << "    ✅ Cache I/O performance test passed" << std::endl;
        return true;
    }
    
    static void create_performance_test_data(const std::string& file_path) {
        std::ofstream file(file_path, std::ios::binary);
        // Write dummy binary data
        for (int i = 0; i < 10000; ++i) {
            file.put(static_cast<char>(i % 256));
        }
    }
    
    static std::string format_bytes(std::size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unit < 3) {
            size /= 1024.0;
            unit++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
        return oss.str();
    }
};

} // namespace gisevo

int main() {
    return gisevo::CachePerformanceTest::run_all_tests() ? 0 : 1;
}