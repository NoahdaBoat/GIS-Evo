#include "map_data.hpp"
#include "binary_loader/cache_manager.hpp"
#include <iostream>

int main() {
    std::cout << "Testing Cache Validation..." << std::endl;
    
    std::string streets_path = "../../resources/maps/ontario.streets.bin";
    std::string osm_path = "../../resources/maps/ontario.osm.bin";
    std::string cache_path = streets_path + ".gisevo.cache";
    
    std::cout << "Streets path: " << streets_path << std::endl;
    std::cout << "OSM path: " << osm_path << std::endl;
    std::cout << "Cache path: " << cache_path << std::endl;
    
    gisevo::CacheManager cache_manager;
    
    std::cout << "Validating cache..." << std::endl;
    auto validation = cache_manager.validate_cache(cache_path, streets_path, osm_path);
    
    std::cout << "Cache exists: " << (validation.exists ? "YES" : "NO") << std::endl;
    std::cout << "Cache valid: " << (validation.valid ? "YES" : "NO") << std::endl;
    std::cout << "Reason: " << validation.reason << std::endl;
    
    if (!validation.streets_checksum.empty()) {
        std::cout << "Streets checksum: " << validation.streets_checksum.substr(0, 16) << "..." << std::endl;
    }
    if (!validation.osm_checksum.empty()) {
        std::cout << "OSM checksum: " << validation.osm_checksum.substr(0, 16) << "..." << std::endl;
    }
    
    return 0;
}
