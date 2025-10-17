#pragma once

#include "map_data.hpp"
#include "types.hpp"
#include <memory>
#include <cairo.h>

namespace gisevo::rendering {

// Clean coordinate transformation system
class CoordinateSystem {
public:
    CoordinateSystem(const gisevo::core::Bounds& map_bounds);
    
    // Convert between coordinate systems
    gisevo::core::Point2D latlon_to_screen(const gisevo::core::LatLon& latlon) const;
    gisevo::core::LatLon screen_to_latlon(const gisevo::core::Point2D& screen) const;
    
    // Get map bounds in screen coordinates
    gisevo::core::Bounds screen_bounds() const;
    
    // Calculate scale for fitting map in viewport
    double calculate_scale(int viewport_width, int viewport_height, double zoom = 1.0) const;

private:
    gisevo::core::Bounds map_bounds_;
    gisevo::core::LatLon map_center_;
    double map_lat_avg_rad_;
    
    static constexpr double kDegreeToRadian = 0.017453292519943295;
    static constexpr double kEarthRadiusInMeters = 6371000.0;
};

// Clean rendering style system
struct RenderStyle {
    double line_width = 1.0;
    double point_size = 2.0;
    struct Color { double r, g, b, a; } color = {0.0, 0.0, 0.0, 1.0};
    bool filled = false;
    bool stroked = true;
};

// Style presets
namespace styles {
    extern const RenderStyle street_default;
    extern const RenderStyle street_highway;
    extern const RenderStyle intersection_default;
    extern const RenderStyle poi_default;
    extern const RenderStyle feature_park;
    extern const RenderStyle feature_water;
    extern const RenderStyle feature_building;
}

// Clean rendering engine
class Renderer {
public:
    Renderer();
    ~Renderer();
    
    // Rendering context management
    void begin_frame(int width, int height, double zoom, const gisevo::core::Point2D& offset);
    void begin_frame(cairo_t* cr, int width, int height, double zoom, const gisevo::core::Point2D& offset);
    void end_frame();
    
    // Coordinate system
    void set_coordinate_system(std::shared_ptr<CoordinateSystem> coords);
    
    // Rendering methods
    void draw_street(const gisevo::core::StreetSegment& street, const RenderStyle& style);
    void draw_intersection(const gisevo::core::Intersection& intersection, const RenderStyle& style);
    void draw_poi(const gisevo::core::POI& poi, const RenderStyle& style);
    void draw_feature(const gisevo::core::Feature& feature, const RenderStyle& style);
    
    // Batch rendering for performance
    void draw_streets(const std::vector<size_t>& street_ids, const gisevo::core::MapData& data, const RenderStyle& style);
    void draw_intersections(const std::vector<size_t>& intersection_ids, const gisevo::core::MapData& data, const RenderStyle& style);
    void draw_pois(const std::vector<size_t>& poi_ids, const gisevo::core::MapData& data, const RenderStyle& style);
    void draw_features(const std::vector<size_t>& feature_ids, const gisevo::core::MapData& data, const RenderStyle& style);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    // Internal rendering state
    int viewport_width_;
    int viewport_height_;
    double zoom_;
    gisevo::core::Point2D offset_;
    std::shared_ptr<CoordinateSystem> coords_;
    
    // Helper methods
    gisevo::core::Point2D transform_point(const gisevo::core::LatLon& latlon) const;
    bool is_visible(const gisevo::core::Point2D& point) const;
};

} // namespace gisevo::rendering
