#include "map_data.hpp"
#include "../spatial_hash/simple_spatial_index.hpp"
#include "../include/bounding_box.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

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

template <typename Fn>
double measure_ms(Fn&& fn) {
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

template <typename T>
bool same_items(std::vector<T> lhs, std::vector<T> rhs) {
    std::sort(lhs.begin(), lhs.end());
    std::sort(rhs.begin(), rhs.end());
    return lhs == rhs;
}

template <typename T>
bool report_difference(const std::vector<T>& lhs, const std::vector<T>& rhs, const char* label) {
    std::vector<T> lhs_sorted = lhs;
    std::vector<T> rhs_sorted = rhs;
    std::sort(lhs_sorted.begin(), lhs_sorted.end());
    std::sort(rhs_sorted.begin(), rhs_sorted.end());

    if (lhs_sorted == rhs_sorted) {
        return true;
    }

    std::vector<T> missing;
    std::set_difference(lhs_sorted.begin(), lhs_sorted.end(), rhs_sorted.begin(), rhs_sorted.end(), std::back_inserter(missing));
    std::vector<T> unexpected;
    std::set_difference(rhs_sorted.begin(), rhs_sorted.end(), lhs_sorted.begin(), lhs_sorted.end(), std::back_inserter(unexpected));

    std::cerr << "Mismatch in " << label << ":" << std::endl;
    if (!missing.empty()) {
        std::cerr << "  Missing " << missing.size() << " ids (showing up to 10):";
        for (std::size_t i = 0; i < std::min<std::size_t>(10, missing.size()); ++i) {
            std::cerr << ' ' << missing[i];
        }
        std::cerr << std::endl;
    }
    if (!unexpected.empty()) {
        std::cerr << "  Unexpected " << unexpected.size() << " ids (showing up to 10):";
        for (std::size_t i = 0; i < std::min<std::size_t>(10, unexpected.size()); ++i) {
            std::cerr << ' ' << unexpected[i];
        }
        std::cerr << std::endl;
    }
    return false;
}

gisevo::BoundingBox make_point_box(double lat, double lon) {
    return gisevo::BoundingBox(lon, lat, lon, lat);
}

gisevo::BoundingBox make_bounds_box(const gisevo::core::Bounds& bounds) {
    return gisevo::BoundingBox(bounds.min_lon, bounds.min_lat, bounds.max_lon, bounds.max_lat);
}

struct Scenario {
    const char* name;
    double lat_span;
    double lon_span;
    bool compare_streets = true;
    bool compare_intersections = true;
    bool compare_pois = true;
    bool compare_features = true;
};

std::vector<std::size_t> filter_street_hits(const gisevo::core::Bounds& query,
                                            const gisevo::core::MapData& map_data,
                                            const std::vector<std::size_t>& candidates) {
    std::vector<std::size_t> filtered;
    filtered.reserve(candidates.size());

    for (auto idx : candidates) {
        if (idx >= map_data.street_count()) {
            continue;
        }

        const auto& segment = map_data.street(idx);
        bool inside = query.contains(segment.from_position) || query.contains(segment.to_position);

        if (!inside) {
            for (const auto& point : segment.curve_points) {
                if (query.contains(point)) {
                    inside = true;
                    break;
                }
            }
        }

        if (inside) {
            filtered.push_back(idx);
        }
    }

    return filtered;
}

std::vector<std::size_t> filter_feature_hits(const gisevo::core::MapData& map_data,
                                             const std::vector<std::size_t>& candidates) {
    std::vector<std::size_t> filtered;
    filtered.reserve(candidates.size());

    for (auto idx : candidates) {
        if (idx >= map_data.feature_count()) {
            continue;
        }
        const auto& feature = map_data.feature(idx);
        if (!feature.points.empty()) {
            filtered.push_back(idx);
        }
    }

    return filtered;
}

} // namespace

int main() {
    using gisevo::core::Bounds;
    using gisevo::core::MapData;
    using gisevo::SimpleSpatialIndex;

    constexpr const char* streets_path = "resources/maps/ontario.streets.bin";
    constexpr const char* osm_path = "resources/maps/ontario.osm.bin";

    const long rss_before_load = read_rss_kb();

    MapData map_data;
    const auto load_start = std::chrono::steady_clock::now();
    if (!map_data.load_from_binary(streets_path, osm_path)) {
        std::cerr << "Failed to load map data" << std::endl;
        return 1;
    }
    const auto load_end = std::chrono::steady_clock::now();

    const double load_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();
    const long rss_after_rtree = read_rss_kb();

    std::cout << "=== Performance Regression Test ===" << std::endl;
    std::cout << "Load time (RTree): " << std::fixed << std::setprecision(2) << load_ms << " ms" << std::endl;

    SimpleSpatialIndex<std::size_t> street_simple;
    SimpleSpatialIndex<std::size_t> intersection_simple;
    SimpleSpatialIndex<std::size_t> poi_simple;
    SimpleSpatialIndex<std::size_t> feature_simple;

    const auto build_simple_ms = measure_ms([&] {
        const auto& streets = map_data.streets();
        for (const auto& street : streets) {
            double min_lat = street.from_position.latitude();
            double max_lat = street.from_position.latitude();
            double min_lon = street.from_position.longitude();
            double max_lon = street.from_position.longitude();

            auto update = [&](const gisevo::core::LatLon& point) {
                min_lat = std::min(min_lat, point.latitude());
                max_lat = std::max(max_lat, point.latitude());
                min_lon = std::min(min_lon, point.longitude());
                max_lon = std::max(max_lon, point.longitude());
            };

            update(street.to_position);
            for (const auto& point : street.curve_points) {
                update(point);
            }

            street_simple.insert(street.id, gisevo::BoundingBox(min_lon, min_lat, max_lon, max_lat));
        }

        const auto& intersections = map_data.intersections();
        for (const auto& intersection : intersections) {
            intersection_simple.insert(intersection.id,
                                       make_point_box(intersection.position.latitude(), intersection.position.longitude()));
        }

        const auto& pois = map_data.pois();
        for (const auto& poi : pois) {
            poi_simple.insert(poi.id, make_point_box(poi.position.latitude(), poi.position.longitude()));
        }

        const auto& features = map_data.features();
        for (const auto& feature : features) {
            if (feature.points.empty()) {
                continue;
            }

            double min_lat = feature.points.front().latitude();
            double max_lat = feature.points.front().latitude();
            double min_lon = feature.points.front().longitude();
            double max_lon = feature.points.front().longitude();

            for (const auto& point : feature.points) {
                min_lat = std::min(min_lat, point.latitude());
                max_lat = std::max(max_lat, point.latitude());
                min_lon = std::min(min_lon, point.longitude());
                max_lon = std::max(max_lon, point.longitude());
            }

            feature_simple.insert(feature.id, gisevo::BoundingBox(min_lon, min_lat, max_lon, max_lat));
        }
    });

    const long rss_after_simple = read_rss_kb();

    const double rtree_mem_mb = (rss_after_rtree - rss_before_load) / 1024.0;
    const double simple_mem_mb = (rss_after_simple - rss_after_rtree) / 1024.0;

    std::cout << "Simple index build time: " << std::fixed << std::setprecision(2) << build_simple_ms << " ms" << std::endl;
    std::cout << "RTree resident memory: " << std::fixed << std::setprecision(2) << rtree_mem_mb << " MB" << std::endl;
    std::cout << "Simple index resident memory (approx): " << std::fixed << std::setprecision(2) << simple_mem_mb << " MB" << std::endl;

    if (map_data.intersection_count() == 0) {
        std::cerr << "No intersections available for query baselines" << std::endl;
        return 1;
    }

    const auto focus = map_data.intersection(0).position;
    const auto bounds = map_data.bounds();

    const std::array<Scenario, 3> scenarios{{
        {"city", 0.05, 0.05, false, true, true, false},
        {"neighborhood", 0.02, 0.02, false, true, true, false},
        {"block", 0.005, 0.005, false, true, true, false}
    }};

    const std::array<std::pair<double, double>, 4> pan_offsets{{
        {0.0, 0.0},
        {0.02, 0.0},
        {0.0, -0.02},
        {-0.02, 0.02}
    }};

    bool success = true;

    for (const auto& scenario : scenarios) {
        double rtree_total_ms = 0.0;
        double simple_total_ms = 0.0;

        std::vector<std::size_t> last_streets;
        std::vector<std::size_t> last_intersections;
        std::vector<std::size_t> last_pois;
        std::vector<std::size_t> last_features;

        for (const auto& offset : pan_offsets) {
            Bounds query{
                clamp(focus.latitude() + offset.first - scenario.lat_span * 0.5, bounds.min_lat, bounds.max_lat),
                clamp(focus.latitude() + offset.first + scenario.lat_span * 0.5, bounds.min_lat, bounds.max_lat),
                clamp(focus.longitude() + offset.second - scenario.lon_span * 0.5, bounds.min_lon, bounds.max_lon),
                clamp(focus.longitude() + offset.second + scenario.lon_span * 0.5, bounds.min_lon, bounds.max_lon)
            };

            std::vector<std::size_t> streets_rtree;
            std::vector<std::size_t> intersections_rtree;
            std::vector<std::size_t> pois_rtree;
            std::vector<std::size_t> features_rtree;

            rtree_total_ms += measure_ms([&] {
                streets_rtree = map_data.streets_in_bounds(query);
                intersections_rtree = map_data.intersections_in_bounds(query);
                pois_rtree = map_data.pois_in_bounds(query);
                features_rtree = map_data.features_in_bounds(query);
            });

            std::vector<std::size_t> streets_simple_result;
            std::vector<std::size_t> intersections_simple_result;
            std::vector<std::size_t> pois_simple_result;
            std::vector<std::size_t> features_simple_result;

            simple_total_ms += measure_ms([&] {
                const auto box = make_bounds_box(query);
                const auto street_candidates = street_simple.query(box);
                streets_simple_result = filter_street_hits(query, map_data, street_candidates);
                intersections_simple_result = intersection_simple.query(box);
                pois_simple_result = poi_simple.query(box);
                const auto feature_candidates = feature_simple.query(box);
                features_simple_result = filter_feature_hits(map_data, feature_candidates);
            });

            bool mismatch = false;
            if (scenario.compare_streets && !report_difference(streets_rtree, streets_simple_result, "streets")) {
                mismatch = true;
            }
            if (scenario.compare_intersections && !report_difference(intersections_rtree, intersections_simple_result, "intersections")) {
                mismatch = true;
            }
            if (scenario.compare_pois && !report_difference(pois_rtree, pois_simple_result, "POIs")) {
                mismatch = true;
            }
            if (scenario.compare_features && !report_difference(features_rtree, features_simple_result, "features")) {
                mismatch = true;
            }

            if (mismatch) {
                success = false;
                std::cerr << "Result mismatch in scenario " << scenario.name << std::endl;
                break;
            }

            last_streets = std::move(streets_rtree);
            last_intersections = std::move(intersections_rtree);
            last_pois = std::move(pois_rtree);
            last_features = std::move(features_rtree);
        }

        if (!success) {
            break;
        }

        const double samples = static_cast<double>(pan_offsets.size());
        const double rtree_avg = rtree_total_ms / samples;
        const double simple_avg = simple_total_ms / samples;

        std::cout << "\nScenario: " << scenario.name << std::endl;
        std::cout << "  RTree avg query time: " << std::fixed << std::setprecision(3) << rtree_avg << " ms" << std::endl;
        std::cout << "  Simple index avg query time: " << std::fixed << std::setprecision(3) << simple_avg << " ms" << std::endl;
        if (simple_avg > 0.0) {
            std::cout << "  Speedup (simple / RTree): " << std::fixed << std::setprecision(2) << (simple_avg / rtree_avg) << "x" << std::endl;
        }

        std::cout << "  Result counts: streets=" << last_streets.size()
                  << ", intersections=" << last_intersections.size()
                  << ", pois=" << last_pois.size()
                  << ", features=" << last_features.size() << std::endl;
    }

    if (!success) {
        return 1;
    }

    const Bounds micro_query{
        clamp(focus.latitude() - 0.001, bounds.min_lat, bounds.max_lat),
        clamp(focus.latitude() + 0.001, bounds.min_lat, bounds.max_lat),
        clamp(focus.longitude() - 0.001, bounds.min_lon, bounds.max_lon),
        clamp(focus.longitude() + 0.001, bounds.min_lon, bounds.max_lon)
    };

    const int iterations = 100;
    double rtree_cache_ms = 0.0;
    double simple_cache_ms = 0.0;

    for (int i = 0; i < iterations; ++i) {
        const double offset = static_cast<double>(i) * 0.0002;
        Bounds query{
            clamp(micro_query.min_lat + offset, bounds.min_lat, bounds.max_lat),
            clamp(micro_query.max_lat + offset, bounds.min_lat, bounds.max_lat),
            clamp(micro_query.min_lon + offset, bounds.min_lon, bounds.max_lon),
            clamp(micro_query.max_lon + offset, bounds.min_lon, bounds.max_lon)
        };

        rtree_cache_ms += measure_ms([&] {
            (void)map_data.streets_in_bounds(query);
        });

        simple_cache_ms += measure_ms([&] {
            street_simple.query(make_bounds_box(query));
        });
    }

    std::cout << "\nCache simulation (" << iterations << " pan steps)" << std::endl;
    std::cout << "  RTree total: " << std::fixed << std::setprecision(3) << rtree_cache_ms << " ms" << std::endl;
    std::cout << "  Simple index total: " << std::fixed << std::setprecision(3) << simple_cache_ms << " ms" << std::endl;

    std::cout << "\n✅ Performance regression checks passed" << std::endl;
    return 0;
}
