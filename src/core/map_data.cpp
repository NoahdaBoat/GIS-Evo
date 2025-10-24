#include "map_data.hpp"

#include "binary_loader/binary_database.hpp"

#include <iostream>

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
    
    // Check if database is already loaded with the same files to prevent double loading
    if (database.get_segment_count() > 0) {
        std::cerr << "Database already loaded, clearing first..." << std::endl;
        database.clear();
    }

    const auto cache_path = streets_path + ".gisevo.cache";
    CacheManager cache_manager;

    if (!database.load_with_cache(streets_path, osm_path, cache_path, cache_manager)) {
        std::cerr << "Failed to load map data with cache" << std::endl;
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

        gisevo::core::Bounds seg_bounds;
        seg_bounds.min_lat = seg_bounds.max_lat = segment.from_position.latitude();
        seg_bounds.min_lon = seg_bounds.max_lon = segment.from_position.longitude();

        auto expand_bounds = [&seg_bounds](const LatLon& point) {
            seg_bounds.min_lat = std::min(seg_bounds.min_lat, point.latitude());
            seg_bounds.max_lat = std::max(seg_bounds.max_lat, point.latitude());
            seg_bounds.min_lon = std::min(seg_bounds.min_lon, point.longitude());
            seg_bounds.max_lon = std::max(seg_bounds.max_lon, point.longitude());
        };

        expand_bounds(segment.to_position);
        for (const auto& point : segment.curve_points) {
            expand_bounds(point);
        }

        segment.bounds = seg_bounds;

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

        if (!feature.points.empty()) {
            gisevo::core::Bounds feature_bounds;
            feature_bounds.min_lat = feature_bounds.max_lat = feature.points.front().latitude();
            feature_bounds.min_lon = feature_bounds.max_lon = feature.points.front().longitude();

            for (const auto& point : feature.points) {
                feature_bounds.min_lat = std::min(feature_bounds.min_lat, point.latitude());
                feature_bounds.max_lat = std::max(feature_bounds.max_lat, point.latitude());
                feature_bounds.min_lon = std::min(feature_bounds.min_lon, point.longitude());
                feature_bounds.max_lon = std::max(feature_bounds.max_lon, point.longitude());
            }

            feature.bounds = feature_bounds;
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

    // Use spatial index for efficient querying
    auto& database = db();
    return database.query_streets_in_bounds(query.min_lon, query.min_lat, query.max_lon, query.max_lat);
}

std::vector<std::size_t> MapData::intersections_in_bounds(const Bounds& query) const {
    if (!query.is_valid()) return {};

    // Use spatial index for efficient querying
    auto& database = db();
    return database.query_intersections_in_bounds(query.min_lon, query.min_lat, query.max_lon, query.max_lat);
}

std::vector<std::size_t> MapData::pois_in_bounds(const Bounds& query) const {
    if (!query.is_valid()) return {};

    // Use spatial index for efficient querying
    auto& database = db();
    return database.query_pois_in_bounds(query.min_lon, query.min_lat, query.max_lon, query.max_lat);
}

std::vector<std::size_t> MapData::features_in_bounds(const Bounds& query) const {
    if (!query.is_valid()) return {};

    // Use spatial index for efficient querying
    auto& database = db();
    return database.query_features_in_bounds(query.min_lon, query.min_lat, query.max_lon, query.max_lat);
}

} // namespace gisevo::core

