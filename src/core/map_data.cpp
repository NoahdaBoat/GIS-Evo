#include "map_data.hpp"

#include "binary_loader/binary_database.hpp"

#include <iostream>
#include <stdexcept>

namespace {

gisevo::BinaryDatabase& db() {
    return gisevo::BinaryDatabase::instance();
}

} // namespace

namespace gisevo::core {

MapData::MapData() = default;

bool MapData::load_from_binary(const std::string& streets_path, const std::string& osm_path) {
    unload();

    auto& database = db();
    database.clear();

    success_ = database.load_streets_file(streets_path);
    if (!success_) {
        std::cerr << "Failed to load streets file: " << streets_path << std::endl;
        return false;
    }

    success_ = database.load_osm_file(osm_path);
    if (!success_) {
        std::cerr << "Failed to load OSM file: " << osm_path << std::endl;
        database.clear();
        return false;
    }

    // Populate local caches
    bounds_.min_lat = database.get_min_lat();
    bounds_.max_lat = database.get_max_lat();
    bounds_.min_lon = database.get_min_lon();
    bounds_.max_lon = database.get_max_lon();
    avg_lat_rad_ = database.get_avg_lat_rad();

    // Streets
    streets_.resize(database.get_segment_count());
    for (std::size_t i = 0; i < streets_.size(); ++i) {
        auto info = database.get_segment_info(i);
        StreetSegment segment;
        segment.id = i;
        segment.street_id = info.streetID;
        segment.from_intersection = info.from;
        segment.to_intersection = info.to;
        segment.one_way = info.oneWay;
        segment.speed_limit_kph = info.speedLimit;
        segment.from_position = database.get_intersection_position(info.from);
        segment.to_position = database.get_intersection_position(info.to);

        segment.curve_points.reserve(info.numCurvePoints);
        for (unsigned j = 0; j < info.numCurvePoints; ++j) {
            segment.curve_points.push_back(database.get_street_segment_curve_point(j, i));
        }

        streets_[i] = std::move(segment);
    }

    // Intersections
    intersections_.resize(database.get_intersection_count());
    for (std::size_t i = 0; i < intersections_.size(); ++i) {
        Intersection intersection;
        intersection.id = i;
        intersection.position = database.get_intersection_position(i);
        intersection.name = database.get_intersection_name(i);
        intersections_[i] = std::move(intersection);
    }

    // POIs
    pois_.resize(database.get_poi_count());
    for (std::size_t i = 0; i < pois_.size(); ++i) {
        POI poi;
        poi.id = i;
        poi.position = database.get_poi_position(i);
        poi.name = database.get_poi_name(i);
        poi.category = database.get_poi_type(i);
        pois_[i] = std::move(poi);
    }

    // Features
    features_.resize(database.get_feature_count());
    for (std::size_t i = 0; i < features_.size(); ++i) {
        Feature feature;
        feature.id = i;
        feature.type = database.get_feature_type(i);
        feature.name = database.get_feature_name(i);
        feature.is_closed = database.get_feature_point_count(i) > 2 &&
                            database.get_feature_point(0, i) == database.get_feature_point(database.get_feature_point_count(i) - 1, i);

        auto point_count = database.get_feature_point_count(i);
        feature.points.reserve(point_count);
        for (std::size_t j = 0; j < point_count; ++j) {
            feature.points.push_back(database.get_feature_point(j, i));
        }

        features_[i] = std::move(feature);
    }

    success_ = true;
    std::cout << "Loaded " << streets_.size() << " streets, "
              << intersections_.size() << " intersections, "
              << pois_.size() << " POIs, and "
              << features_.size() << " features." << std::endl;
    return true;
}

void MapData::unload() {
    streets_.clear();
    intersections_.clear();
    pois_.clear();
    features_.clear();
    success_ = false;
}

std::vector<std::size_t> MapData::streets_in_bounds(const Bounds& query) const {
    if (!query.is_valid()) return {};

    const auto& database = db();
    auto lon_lat_indices = database.query_streets_in_bounds(query.min_lon, query.min_lat, query.max_lon, query.max_lat);

    std::vector<std::size_t> filtered;
    filtered.reserve(std::min(lon_lat_indices.size(), streets_.size()));

    for (std::size_t idx : lon_lat_indices) {
        if (idx >= streets_.size()) {
            continue;
        }

        const auto& segment = streets_[idx];

        bool inside = query.contains(segment.from_position) ||
                       query.contains(segment.to_position);

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

std::vector<std::size_t> MapData::intersections_in_bounds(const Bounds& query) const {
    if (!query.is_valid()) return {};

    const auto& database = db();
    return database.query_intersections_in_bounds(query.min_lon, query.min_lat, query.max_lon, query.max_lat);
}

std::vector<std::size_t> MapData::pois_in_bounds(const Bounds& query) const {
    if (!query.is_valid()) return {};

    const auto& database = db();
    return database.query_pois_in_bounds(query.min_lon, query.min_lat, query.max_lon, query.max_lat);
}

std::vector<std::size_t> MapData::features_in_bounds(const Bounds& query) const {
    if (!query.is_valid()) return {};

    const auto& database = db();
    return database.query_features_in_bounds(query.min_lon, query.min_lat, query.max_lon, query.max_lat);
}

} // namespace gisevo::core

