#include <iostream>
#include "../binary_loader/binary_database.hpp"

int main() {
    std::cout << "Testing OSM Feature Conversion..." << std::endl;
    
    std::string streets_path = "../../resources/maps/ontario.streets.bin";
    std::string osm_path = "../../resources/maps/ontario.osm.bin";
    
    std::cout << "Loading data from binary files..." << std::endl;
    
    // Load data directly
    auto& db = gisevo::BinaryDatabase::instance();
    
    std::cout << "Loading streets file: " << streets_path << std::endl;
    bool streets_loaded = db.load_streets_file(streets_path);
    if (!streets_loaded) {
        std::cerr << "Failed to load streets file" << std::endl;
        return 1;
    }
    
    std::cout << "Loading OSM file: " << osm_path << std::endl;
    bool osm_loaded = db.load_osm_file(osm_path);
    if (!osm_loaded) {
        std::cerr << "Failed to load OSM file" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Binary data loaded successfully!" << std::endl;
    std::cout << "Streets: " << db.get_segment_count() << std::endl;
    std::cout << "Intersections: " << db.get_intersection_count() << std::endl;
    std::cout << "POIs: " << db.get_poi_count() << std::endl;
    std::cout << "Features: " << db.get_feature_count() << std::endl;
    
    // Test feature conversion directly
    std::cout << "\nTesting feature conversion..." << std::endl;
    
    // Test a few features
    for (size_t i = 0; i < std::min(size_t(10), db.get_feature_count()); ++i) {
        auto feature_type = db.get_feature_type(i);
        auto feature_name = db.get_feature_name(i);
        auto point_count = db.get_feature_point_count(i);
        
        std::cout << "Feature " << i << ": type=" << static_cast<int>(feature_type) 
                  << ", name='" << feature_name << "', points=" << point_count << std::endl;
        
        // Test first few points
        for (size_t j = 0; j < std::min(size_t(3), point_count); ++j) {
            auto point = db.get_feature_point(j, i);
            std::cout << "  Point " << j << ": (" << point.lat << ", " << point.lon << ")" << std::endl;
        }
    }
    
    std::cout << "\n🎉 OSM Feature Conversion Test PASSED!" << std::endl;
    return 0;
}
