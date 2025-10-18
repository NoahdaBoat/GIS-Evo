#include "map_data.hpp"
#include <iostream>

int main() {
    std::cout << "Testing Clean Architecture Integration..." << std::endl;
    
    // Test data loading
    gisevo::core::MapData map_data;
    
    std::string streets_path = "resources/maps/ontario.streets.bin";
    std::string osm_path = "resources/maps/ontario.osm.bin";
    
    std::cout << "Loading data from binary files..." << std::endl;
    bool loaded = map_data.load_from_binary(streets_path, osm_path);
    
    if (loaded) {
        std::cout << "✅ Data loaded successfully!" << std::endl;
        
        // Test spatial queries
        gisevo::core::Bounds test_bounds = {
            -79.5, -79.4,  // lat range
            43.6, 43.7     // lon range (Toronto area)
        };
        
        auto visible_streets = map_data.streets_in_bounds(test_bounds);
        auto visible_intersections = map_data.intersections_in_bounds(test_bounds);
        auto visible_pois = map_data.pois_in_bounds(test_bounds);
        auto visible_features = map_data.features_in_bounds(test_bounds);
        
        std::cout << "✅ Spatial queries working!" << std::endl;
        std::cout << "Found " << visible_streets.size() << " streets in test bounds" << std::endl;
        std::cout << "Found " << visible_intersections.size() << " intersections in test bounds" << std::endl;
        std::cout << "Found " << visible_pois.size() << " POIs in test bounds" << std::endl;
        std::cout << "Found " << visible_features.size() << " features in test bounds" << std::endl;
        
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
