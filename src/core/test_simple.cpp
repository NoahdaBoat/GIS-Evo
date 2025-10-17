#include "map_data.hpp"
#include <iostream>

int main() {
    std::cout << "Testing Clean Architecture Integration (Simplified)..." << std::endl;
    
    // Test data loading
    gisevo::core::MapData map_data;
    
    std::string streets_path = "../../resources/maps/ontario.streets.bin";
    std::string osm_path = "../../resources/maps/ontario.osm.bin";
    
    std::cout << "Loading data from binary files..." << std::endl;
    bool loaded = map_data.load_from_binary(streets_path, osm_path);
    
    if (loaded) {
        std::cout << "✅ Data loaded successfully!" << std::endl;
        
        // Test basic data access
        std::cout << "✅ MapData created successfully" << std::endl;
        
        // Test coordinate system
        auto bounds = map_data.bounds();
        std::cout << "✅ Map bounds: lat[" << bounds.min_lat << ", " << bounds.max_lat 
                  << "], lon[" << bounds.min_lon << ", " << bounds.max_lon << "]" << std::endl;
        
        std::cout << "\n🎉 Clean Architecture Integration Test PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Failed to load data" << std::endl;
        return 1;
    }
}
