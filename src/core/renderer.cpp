#include "renderer.hpp"

#include <cairo.h>
#include <cmath>

namespace gisevo::rendering {

// CoordinateSystem implementation
CoordinateSystem::CoordinateSystem(const gisevo::core::Bounds& map_bounds) 
    : map_bounds_(map_bounds) {
    map_center_ = map_bounds.center();
    map_lat_avg_rad_ = map_center_.lat * kDegreeToRadian;
}

gisevo::core::Point2D CoordinateSystem::latlon_to_screen(const gisevo::core::LatLon& latlon) const {
    const double lat_delta_rad = kDegreeToRadian * (latlon.lat - map_center_.lat);
    const double lon_delta_rad = kDegreeToRadian * (latlon.lon - map_center_.lon);

    double x = kEarthRadiusInMeters * lon_delta_rad * cos(map_lat_avg_rad_);
    double y = kEarthRadiusInMeters * lat_delta_rad;

    return gisevo::core::Point2D{x, y};
}

gisevo::core::LatLon CoordinateSystem::screen_to_latlon(const gisevo::core::Point2D& screen) const {
    double lat_rad = screen.y / kEarthRadiusInMeters;
    double lon_rad = screen.x / (kEarthRadiusInMeters * cos(map_lat_avg_rad_));

    double lat = map_center_.lat + (lat_rad / kDegreeToRadian);
    double lon = map_center_.lon + (lon_rad / kDegreeToRadian);

    return gisevo::core::LatLon(lat, lon);
}

gisevo::core::Bounds CoordinateSystem::screen_bounds() const {
    auto min_screen = latlon_to_screen(gisevo::core::LatLon(map_bounds_.min_lat, map_bounds_.min_lon));
    auto max_screen = latlon_to_screen(gisevo::core::LatLon(map_bounds_.max_lat, map_bounds_.max_lon));
    
    return gisevo::core::Bounds{
        min_screen.y, max_screen.y,  // lat -> y
        min_screen.x, max_screen.x   // lon -> x
    };
}

double CoordinateSystem::calculate_scale(int viewport_width, int viewport_height, double zoom) const {
    auto bounds = screen_bounds();
    double map_width = bounds.max_lon - bounds.min_lon;
    double map_height = bounds.max_lat - bounds.min_lat;
    
    double scale_x = (viewport_width * 0.8) / map_width;
    double scale_y = (viewport_height * 0.8) / map_height;
    
    return std::min(scale_x, scale_y) * zoom;
}

// RenderStyle definitions
namespace styles {
    const RenderStyle street_default{
        .line_width = 2.5,
        .color = {0.6, 0.6, 0.6, 1.0},  // Light gray for default roads
        .filled = false,
        .stroked = true
    };
    
    const RenderStyle street_highway{
        .line_width = 4.0,
        .color = {0.95, 0.65, 0.2, 1.0},  // Orange/yellow for highways
        .filled = false,
        .stroked = true
    };
    
    const RenderStyle intersection_default{
        .point_size = 3.0,
        .color = {1.0, 0.0, 0.0, 1.0},
        .filled = true,
        .stroked = false
    };
    
    const RenderStyle poi_default{
        .point_size = 2.0,
        .color = {0.0, 0.0, 1.0, 1.0},
        .filled = true,
        .stroked = false
    };
    
    const RenderStyle feature_park{
        .color = {0.55, 0.8, 0.4, 0.6},  // Proper green for parks with better opacity
        .filled = true,
        .stroked = true
    };
    
    const RenderStyle feature_water{
        .color = {0.4, 0.7, 0.9, 0.7},  // Proper blue for water with better opacity
        .filled = true,
        .stroked = true
    };
    
    const RenderStyle feature_building{
        .color = {0.7, 0.65, 0.6, 0.5},  // Beige/tan for buildings with better opacity
        .filled = true,
        .stroked = true
    };
}

// Renderer implementation
struct Renderer::Impl {
    cairo_t* cr = nullptr;
    int viewport_width = 0;
    int viewport_height = 0;
    double zoom = 1.0;
    gisevo::core::Point2D offset{0, 0};
    std::shared_ptr<CoordinateSystem> coords;
};

Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}

Renderer::~Renderer() = default;

void Renderer::begin_frame(int width, int height, double zoom, const gisevo::core::Point2D& offset) {
    impl_->viewport_width = width;
    impl_->viewport_height = height;
    impl_->zoom = zoom;
    impl_->offset = offset;
}

void Renderer::begin_frame(cairo_t* cr, int width, int height, double zoom, const gisevo::core::Point2D& offset) {
    impl_->cr = cr;
    impl_->viewport_width = width;
    impl_->viewport_height = height;
    impl_->zoom = zoom;
    impl_->offset = offset;
}

void Renderer::end_frame() {
    impl_->cr = nullptr;
}

void Renderer::set_coordinate_system(std::shared_ptr<CoordinateSystem> coords) {
    impl_->coords = coords;
}

void Renderer::draw_street(const gisevo::core::StreetSegment& street, const RenderStyle& style) {
    if (!impl_->cr || !impl_->coords) return;
    
    // Set style
    cairo_set_line_width(impl_->cr, style.line_width / impl_->zoom);
    cairo_set_source_rgba(impl_->cr, style.color.r, style.color.g, style.color.b, style.color.a);
    
    // Draw street segment using actual data
    if (!street.curve_points.empty()) {
        // Draw with curve points
        auto first_point = transform_point(street.curve_points[0]);
        cairo_move_to(impl_->cr, first_point.x, first_point.y);
        
        for (size_t i = 1; i < street.curve_points.size(); ++i) {
            auto transformed = transform_point(street.curve_points[i]);
            cairo_line_to(impl_->cr, transformed.x, transformed.y);
        }
    } else {
        // Draw straight line between intersections using from_position and to_position
        auto from_point = transform_point(street.from_position);
        auto to_point = transform_point(street.to_position);
        cairo_move_to(impl_->cr, from_point.x, from_point.y);
        cairo_line_to(impl_->cr, to_point.x, to_point.y);
    }
    
    if (style.stroked) {
        cairo_stroke(impl_->cr);
    }
}

void Renderer::draw_intersection(const gisevo::core::Intersection& intersection, const RenderStyle& style) {
    if (!impl_->cr || !impl_->coords) return;
    
    auto transformed = transform_point(intersection.position);
    
    cairo_set_source_rgba(impl_->cr, style.color.r, style.color.g, style.color.b, style.color.a);
    cairo_arc(impl_->cr, transformed.x, transformed.y, style.point_size / impl_->zoom, 0, 2 * M_PI);
    
    if (style.filled) {
        cairo_fill(impl_->cr);
    }
    if (style.stroked) {
        cairo_stroke(impl_->cr);
    }
}

void Renderer::draw_poi(const gisevo::core::POI& poi, const RenderStyle& style) {
    if (!impl_->cr || !impl_->coords) return;
    
    auto transformed = transform_point(poi.position);
    
    cairo_set_source_rgba(impl_->cr, style.color.r, style.color.g, style.color.b, style.color.a);
    cairo_arc(impl_->cr, transformed.x, transformed.y, style.point_size / impl_->zoom, 0, 2 * M_PI);
    
    if (style.filled) {
        cairo_fill(impl_->cr);
    }
    if (style.stroked) {
        cairo_stroke(impl_->cr);
    }
}

void Renderer::draw_feature(const gisevo::core::Feature& feature, const RenderStyle& style) {
    if (!impl_->cr || !impl_->coords || feature.points.size() < 3) return;
    
    cairo_set_source_rgba(impl_->cr, style.color.r, style.color.g, style.color.b, style.color.a);
    
    bool first_point = true;
    for (const auto& point : feature.points) {
        auto transformed = transform_point(point);
        
        if (first_point) {
            cairo_move_to(impl_->cr, transformed.x, transformed.y);
            first_point = false;
        } else {
            cairo_line_to(impl_->cr, transformed.x, transformed.y);
        }
    }
    
    cairo_close_path(impl_->cr);
    
    if (style.filled) {
        cairo_fill(impl_->cr);
    }
    if (style.stroked) {
        cairo_stroke(impl_->cr);
    }
}

void Renderer::draw_streets(const std::vector<size_t>& street_ids, const gisevo::core::MapData& data, const RenderStyle& style) {
    for (size_t id : street_ids) {
        if (id < data.street_count()) {
            draw_street(data.street(id), style);
        }
    }
}

void Renderer::draw_intersections(const std::vector<size_t>& intersection_ids, const gisevo::core::MapData& data, const RenderStyle& style) {
    for (size_t id : intersection_ids) {
        if (id < data.intersection_count()) {
            draw_intersection(data.intersection(id), style);
        }
    }
}

void Renderer::draw_pois(const std::vector<size_t>& poi_ids, const gisevo::core::MapData& data, const RenderStyle& style) {
    for (size_t id : poi_ids) {
        if (id < data.poi_count()) {
            draw_poi(data.poi(id), style);
        }
    }
}

void Renderer::draw_features(const std::vector<size_t>& feature_ids, const gisevo::core::MapData& data, const RenderStyle& style) {
    for (size_t id : feature_ids) {
        if (id < data.feature_count()) {
            draw_feature(data.feature(id), style);
        }
    }
}

gisevo::core::Point2D Renderer::transform_point(const gisevo::core::LatLon& latlon) const {
    if (!impl_->coords) return gisevo::core::Point2D{0, 0};
    
    auto screen_pos = impl_->coords->latlon_to_screen(latlon);
    
    // Apply viewport transformation
    double x = screen_pos.x * impl_->zoom + impl_->offset.x + impl_->viewport_width / 2.0;
    double y = screen_pos.y * impl_->zoom + impl_->offset.y + impl_->viewport_height / 2.0;
    
    return gisevo::core::Point2D{x, y};
}

bool Renderer::is_visible(const gisevo::core::Point2D& point) const {
    return point.x >= -50 && point.x <= impl_->viewport_width + 50 &&
           point.y >= -50 && point.y <= impl_->viewport_height + 50;
}

} // namespace gisevo::rendering
