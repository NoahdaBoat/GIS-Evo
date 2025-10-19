#include "cache_manager.hpp"
#include "binary_database.hpp"
#include <iostream>
#include <fstream>
#include <cassert>

namespace gisevo {

void test_cache_format() {
    std::cout << "Testing cache file format..." << std::endl;
    
    CacheManager cache_manager;
    BinaryDatabase& db = BinaryDatabase::instance();
    
    // Clear any existing data
    db.clear();
    
    // Create test data
    BinaryDatabase::Node node1;
    node1.osm_id = 12345;
    node1.lat = 43.6532;
    node1.lon = -79.3832;
    node1.tags = {{"highway", "primary"}, {"name", "Test Street"}};
    db.add_node(node1);
    
    BinaryDatabase::Node node2;
    node2.osm_id = 67890;
    node2.lat = 43.6542;
    node2.lon = -79.3842;
    node2.tags = {{"highway", "primary"}, {"name", "Test Street"}};
    db.add_node(node2);
    
    BinaryDatabase::StreetSegment segment;
    segment.osm_id = 11111;
    segment.category = 1;
    segment.max_speed_kph = 50.0f;
    segment.name = "Test Street";
    segment.node_refs = {12345, 67890};
    segment.is_closed = false;
    segment.tags = {{"highway", "primary"}, {"name", "Test Street"}};
    db.add_segment(segment);
    
    BinaryDatabase::POI poi;
    poi.osm_id = 22222;
    poi.lat = 43.6537;
    poi.lon = -79.3837;
    poi.category = "restaurant";
    poi.name = "Test Restaurant";
    poi.tags = {{"amenity", "restaurant"}, {"name", "Test Restaurant"}};
    db.add_poi(poi);
    
    // Set map bounds
    db.set_map_bounds(43.6532, 43.6542, -79.3842, -79.3832, 0.7618);
    
    // Build spatial indexes
    db.rebuild_spatial_indexes();
    
    // Test cache save
    const std::string cache_path = "/tmp/test_cache.gisevo.cache";
    const std::string streets_checksum = "test_streets_checksum";
    const std::string osm_checksum = "test_osm_checksum";
    
    std::cout << "Saving cache..." << std::endl;
    bool save_success = cache_manager.save_cache(cache_path, db, streets_checksum, osm_checksum);
    assert(save_success);
    std::cout << "Cache saved successfully" << std::endl;
    
    // Clear database
    db.clear();
    
    // Test cache load
    std::cout << "Loading cache..." << std::endl;
    bool load_success = cache_manager.load_cache(cache_path, db);
    assert(load_success);
    std::cout << "Cache loaded successfully" << std::endl;
    
    // Verify data integrity
    std::cout << "Verifying data integrity..." << std::endl;
    
    // Check nodes
    assert(db.get_node_count() == 2);
    assert(db.get_node_by_index(0).osm_id == 12345);
    assert(db.get_node_by_index(0).lat == 43.6532);
    assert(db.get_node_by_index(0).lon == -79.3832);
    assert(db.get_node_by_index(1).osm_id == 67890);
    
    // Check segments
    assert(db.get_segment_count() == 1);
    assert(db.get_segment_by_index(0).osm_id == 11111);
    assert(db.get_segment_by_index(0).name == "Test Street");
    assert(db.get_segment_by_index(0).node_refs.size() == 2);
    assert(db.get_segment_by_index(0).node_refs[0] == 12345);
    assert(db.get_segment_by_index(0).node_refs[1] == 67890);
    
    // Check POIs
    assert(db.get_poi_count() == 1);
    assert(db.get_poi_name(0) == "Test Restaurant");
    assert(db.get_poi_type(0) == "restaurant");
    
    // Check map bounds
    assert(db.get_min_lat() == 43.6532);
    assert(db.get_max_lat() == 43.6542);
    assert(db.get_min_lon() == -79.3842);
    assert(db.get_max_lon() == -79.3832);
    
    // Check spatial indexes
    assert(db.get_street_rtree().size() == 1);
    assert(db.get_poi_rtree().size() == 1);
    
    std::cout << "All tests passed!" << std::endl;
    
    // Clean up
    std::remove(cache_path.c_str());
}

} // namespace gisevo

int main() {
    try {
        gisevo::test_cache_format();
        std::cout << "Cache format test completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << std::endl;
        return 1;
    }
}
