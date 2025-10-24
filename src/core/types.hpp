#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "../LatLon.h"
#include "../StreetsDatabaseAPI.h"

namespace gisevo::core {

using LatLon = ::LatLon;

struct Point2D {
    double x = 0.0;
    double y = 0.0;
    Point2D() = default;
    Point2D(double x_val, double y_val) : x(x_val), y(y_val) {}
};

struct Bounds {
    double min_lat = 0.0;
    double max_lat = 0.0;
    double min_lon = 0.0;
    double max_lon = 0.0;

    [[nodiscard]] bool is_valid() const {
        return min_lat <= max_lat && min_lon <= max_lon;
    }

    [[nodiscard]] LatLon center() const {
        return LatLon((min_lat + max_lat) * 0.5, (min_lon + max_lon) * 0.5);
    }

    [[nodiscard]] bool contains(const LatLon& point) const {
        return point.latitude() >= min_lat && point.latitude() <= max_lat &&
               point.longitude() >= min_lon && point.longitude() <= max_lon;
    }

    [[nodiscard]] bool intersects(const Bounds& other) const {
        return !(other.min_lon > max_lon || other.max_lon < min_lon ||
                 other.min_lat > max_lat || other.max_lat < min_lat);
    }
};

struct StreetSegment {
    std::size_t id = 0;
    std::size_t street_id = 0;
    std::size_t from_intersection = 0;
    std::size_t to_intersection = 0;
    bool one_way = false;
    double speed_limit_kph = 0.0;
    LatLon from_position;
    LatLon to_position;
    std::vector<LatLon> curve_points;
    Bounds bounds{};
};

struct Intersection {
    std::size_t id = 0;
    LatLon position;
    std::string name;
};

struct POI {
    std::size_t id = 0;
    LatLon position;
    std::string name;
    std::string category;
};

struct Feature {
    std::size_t id = 0;
    ::FeatureType type = ::FeatureType::UNKNOWN;
    std::string name;
    bool is_closed = false;
    std::vector<LatLon> points;
    Bounds bounds{};
};

} // namespace gisevo::core
