#include "map_data.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

double clamp(double value, double min_val, double max_val) {
    return std::max(min_val, std::min(max_val, value));
}

long read_rss_kb() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line.substr(6));
            long rss_kb = 0;
            iss >> rss_kb;
            return rss_kb;
        }
    }
    return -1;
}

} // namespace

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
        
        auto bounds = map_data.bounds();
        std::cout << "✅ Map bounds: lat[" << bounds.min_lat << ", " << bounds.max_lat 
                  << "], lon[" << bounds.min_lon << ", " << bounds.max_lon << "]" << std::endl;

        long rss_kb = read_rss_kb();
        if (rss_kb > 0) {
            std::cout << "Resident memory usage: " << rss_kb / 1024.0 << " MB" << std::endl;
        }

        const auto& focus_intersection = map_data.intersection(0);
        const auto center_lat = focus_intersection.position.latitude();
        const auto center_lon = focus_intersection.position.longitude();

        struct Scenario {
            const char* name;
            double lat_span;
            double lon_span;
        };

        std::array<Scenario, 3> scenarios{Scenario{"city", 0.05, 0.05},
                                           Scenario{"neighborhood", 0.02, 0.02},
                                           Scenario{"block", 0.005, 0.005}};

        std::array<std::pair<double, double>, 4> pan_offsets{{{0.0, 0.0}, {0.02, 0.0}, {0.0, -0.02}, {-0.02, 0.02}}};

        for (const auto& scenario : scenarios) {
            std::cout << "\nScenario: " << scenario.name << std::endl;

            double total_ms = 0.0;
            std::size_t last_street_count = 0;
            std::size_t last_intersection_count = 0;
            std::size_t last_poi_count = 0;
            std::size_t last_feature_count = 0;

            for (const auto& offset : pan_offsets) {
                gisevo::core::Bounds query{
                    clamp(center_lat + offset.first - scenario.lat_span * 0.5, bounds.min_lat, bounds.max_lat),
                    clamp(center_lat + offset.first + scenario.lat_span * 0.5, bounds.min_lat, bounds.max_lat),
                    clamp(center_lon + offset.second - scenario.lon_span * 0.5, bounds.min_lon, bounds.max_lon),
                    clamp(center_lon + offset.second + scenario.lon_span * 0.5, bounds.min_lon, bounds.max_lon)
                };

                auto start = std::chrono::steady_clock::now();
                auto streets = map_data.streets_in_bounds(query);
                auto intersections = map_data.intersections_in_bounds(query);
                auto pois = map_data.pois_in_bounds(query);
                auto features = map_data.features_in_bounds(query);
                auto end = std::chrono::steady_clock::now();

                last_street_count = streets.size();
                last_intersection_count = intersections.size();
                last_poi_count = pois.size();
                last_feature_count = features.size();

                const double ms = std::chrono::duration<double, std::milli>(end - start).count();
                total_ms += ms;
            }

            const double avg_ms = total_ms / static_cast<double>(pan_offsets.size());
            std::cout << "Average query time: " << avg_ms << " ms" << std::endl;
            std::cout << "Last street count: " << last_street_count << std::endl;
            std::cout << "Last intersection count: " << last_intersection_count << std::endl;
            std::cout << "Last POI count: " << last_poi_count << std::endl;
            std::cout << "Last feature count: " << last_feature_count << std::endl;
        }

        std::cout << "\n🎉 Clean Architecture Integration Test PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Failed to load data" << std::endl;
        return 1;
    }
}
