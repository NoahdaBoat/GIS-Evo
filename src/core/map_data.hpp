#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include "types.hpp"

namespace gisevo {
    class BinaryDatabase;
    class CacheManager;
}

namespace gisevo::core {

class MapData {
public:
    MapData();

    bool load_from_binary(const std::string& streets_path, const std::string& osm_path);
    void unload();

    [[nodiscard]] bool load_success() const { return success_; }

    [[nodiscard]] Bounds bounds() const { return bounds_; }
    [[nodiscard]] double average_latitude_rad() const { return avg_lat_rad_; }

    [[nodiscard]] std::size_t street_count() const { return streets_.size(); }
    [[nodiscard]] std::size_t intersection_count() const { return intersections_.size(); }
    [[nodiscard]] std::size_t poi_count() const { return pois_.size(); }
    [[nodiscard]] std::size_t feature_count() const { return features_.size(); }

    [[nodiscard]] const StreetSegment& street(std::size_t idx) const { return streets_.at(idx); }
    [[nodiscard]] const Intersection& intersection(std::size_t idx) const { return intersections_.at(idx); }
    [[nodiscard]] const POI& poi(std::size_t idx) const { return pois_.at(idx); }
    [[nodiscard]] const Feature& feature(std::size_t idx) const { return features_.at(idx); }

    [[nodiscard]] const std::vector<StreetSegment>& streets() const { return streets_; }
    [[nodiscard]] const std::vector<Intersection>& intersections() const { return intersections_; }
    [[nodiscard]] const std::vector<POI>& pois() const { return pois_; }
    [[nodiscard]] const std::vector<Feature>& features() const { return features_; }

    [[nodiscard]] std::vector<std::size_t> streets_in_bounds(const Bounds& query) const;
    [[nodiscard]] std::vector<std::size_t> intersections_in_bounds(const Bounds& query) const;
    [[nodiscard]] std::vector<std::size_t> pois_in_bounds(const Bounds& query) const;
    [[nodiscard]] std::vector<std::size_t> features_in_bounds(const Bounds& query) const;

private:
    bool success_ = false;
    Bounds bounds_{};
    double avg_lat_rad_ = 0.0;

    std::vector<StreetSegment> streets_;
    std::vector<Intersection> intersections_;
    std::vector<POI> pois_;
    std::vector<Feature> features_;
};

} // namespace gisevo::core
