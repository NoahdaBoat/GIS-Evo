#include "map_view.hpp"

#include <algorithm>
#include <cmath>
#include <gdk/gdkkeysyms.h>
#include <iostream>
#include "StreetsDatabaseAPI.h"

namespace {
constexpr double kZoomStep = 1.1;
constexpr double kMinZoom = 0.25;
constexpr double kMaxZoom = 8.0;
constexpr double kPanStep = 32.0;
constexpr double LARGE_FEATURE_THRESHOLD = 0.01; // ~1% of map area
}

MapView::MapView(std::shared_ptr<gisevo::core::MapData> map_data)
    : drawing_area_(gtk_drawing_area_new())
    , map_data_(map_data)
    , offset_x_(0.0)
    , offset_y_(0.0)
    , zoom_(1.0)  // Start with normal zoom level for debugging
    , drag_start_x_(0.0)
    , drag_start_y_(0.0)
    , renderer_(std::make_unique<gisevo::rendering::Renderer>())
    , coordinate_system_(nullptr)
{
  gtk_widget_set_hexpand(drawing_area_, TRUE);
  gtk_widget_set_vexpand(drawing_area_, TRUE);

  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area_), MapView::draw_cb, this, nullptr);

  GtkGesture *drag = gtk_gesture_drag_new();
  gtk_widget_add_controller(drawing_area_, GTK_EVENT_CONTROLLER(drag));
  g_signal_connect(drag, "drag-begin", G_CALLBACK(MapView::drag_begin_cb), this);
  g_signal_connect(drag, "drag-update", G_CALLBACK(MapView::drag_update_cb), this);

  GtkEventController *scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  gtk_widget_add_controller(drawing_area_, scroll);
  g_signal_connect(scroll, "scroll", G_CALLBACK(MapView::scroll_cb), this);

  GtkEventController *key = gtk_event_controller_key_new();
  gtk_widget_add_controller(drawing_area_, key);
  g_signal_connect(key, "key-pressed", G_CALLBACK(MapView::key_press_cb), this);
  
  ensure_coordinate_system();
}

GtkWidget *MapView::widget() const
{
  return drawing_area_;
}

void MapView::draw(cairo_t *cr, int width, int height)
{
  cairo_save(cr);
  
  // Safety check: ensure map data is valid before drawing
  if (!map_data_ || !map_data_->load_success()) {
    // Draw loading indicator or blank screen
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);  // Light gray background
    cairo_paint(cr);
    
    // Draw a simple "Loading..." text
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);  // Dark gray text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 24);
    cairo_move_to(cr, width / 2.0 - 50, height / 2.0);
    cairo_show_text(cr, "Loading...");
    
    cairo_restore(cr);
    return;
  }
  
  // Additional safety check: ensure we have features to render
  if (map_data_->feature_count() == 0) {
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);  // Light gray background
    cairo_paint(cr);
    cairo_restore(cr);
    return;
  }
  
  ensure_coordinate_system();

  if (!renderer_ || !coordinate_system_) {
    cairo_restore(cr);
    return;
  }

  // Set background color to a light gray
  cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
  cairo_paint(cr);

  const auto bounds = map_data_->bounds();
  const double map_center_lat = (bounds.min_lat + bounds.max_lat) * 0.5;
  const double map_center_lon = (bounds.min_lon + bounds.max_lon) * 0.5;
  const gisevo::core::Point2D map_center_screen = latlon_to_screen(gisevo::core::LatLon(map_center_lat, map_center_lon));

  const gisevo::core::Point2D min_screen = latlon_to_screen(gisevo::core::LatLon(bounds.min_lat, bounds.min_lon));
  const gisevo::core::Point2D max_screen = latlon_to_screen(gisevo::core::LatLon(bounds.max_lat, bounds.max_lon));

  const double map_width = std::max(std::abs(max_screen.x - min_screen.x), 1.0);
  const double map_height = std::max(std::abs(max_screen.y - min_screen.y), 1.0);
  const double scale_x = (width * 0.8) / map_width;
  const double scale_y = (height * 0.8) / map_height;
  const double base_scale = std::min(scale_x, scale_y);
  const double final_scale = std::max(base_scale * zoom_, 1e-6);

  ViewportBounds viewport_bounds = get_viewport_bounds(width, height, final_scale, map_center_screen);
  viewport_cache_.ensure(viewport_bounds, *map_data_);

  const gisevo::core::Point2D renderer_offset{
      offset_x_ - map_center_screen.x * final_scale,
      offset_y_ - map_center_screen.y * final_scale};

  renderer_->begin_frame(cr, width, height, final_scale, renderer_offset);

  // Draw features first (as background)
  draw_features_with_renderer();
  
  // Draw streets on top of features
  draw_streets_with_renderer();
  
  // Draw intersections and POIs on top
  draw_intersections_with_renderer();
  draw_pois_with_renderer();

  renderer_->end_frame();
  cairo_restore(cr);
}

// ViewportCache implementation
bool MapView::ViewportCache::should_invalidate(const ViewportBounds& new_bounds, double zoom_change_threshold) {
  if (!is_valid) return true;
  
  // Check if bounds changed significantly
  double bounds_change = std::max(
    std::abs(new_bounds.min_lon - cached_bounds.min_lon),
    std::abs(new_bounds.max_lon - cached_bounds.max_lon)
  );
  bounds_change = std::max(bounds_change, std::abs(new_bounds.min_lat - cached_bounds.min_lat));
  bounds_change = std::max(bounds_change, std::abs(new_bounds.max_lat - cached_bounds.max_lat));
  
  // Invalidate if bounds changed by more than 10% of viewport size
  double viewport_size = std::max(
    cached_bounds.max_lon - cached_bounds.min_lon,
    cached_bounds.max_lat - cached_bounds.min_lat
  );
  
  return bounds_change > (viewport_size * 0.1);
}

void MapView::ViewportCache::update(const ViewportBounds& bounds, const gisevo::core::MapData& map_data) {
  cached_bounds = bounds;
  // Query spatial data for the new bounds
  cached_streets = map_data.streets_in_bounds({bounds.min_lat, bounds.max_lat, bounds.min_lon, bounds.max_lon});
  cached_intersections = map_data.intersections_in_bounds({bounds.min_lat, bounds.max_lat, bounds.min_lon, bounds.max_lon});
  cached_pois = map_data.pois_in_bounds({bounds.min_lat, bounds.max_lat, bounds.min_lon, bounds.max_lon});
  cached_features = map_data.features_in_bounds({bounds.min_lat, bounds.max_lat, bounds.min_lon, bounds.max_lon});
  
  is_valid = true;
}

void MapView::ViewportCache::invalidate() {
  is_valid = false;
  cached_streets.clear();
  cached_intersections.clear();
  cached_pois.clear();
  cached_features.clear();
}

void MapView::ViewportCache::ensure(const ViewportBounds& bounds, const gisevo::core::MapData& map_data) {
  if (should_invalidate(bounds)) {
    update(bounds, map_data);
  }
}

void MapView::begin_drag(double, double)
{
  drag_start_x_ = offset_x_;
  drag_start_y_ = offset_y_;
}

void MapView::update_drag(double x, double y)
{
  offset_x_ = drag_start_x_ + x;
  offset_y_ = drag_start_y_ + y;
  
  // Throttle redraws - only redraw every N pixels of movement
  static double last_redraw_x = 0.0;
  static double last_redraw_y = 0.0;
  double dx = x - last_redraw_x;
  double dy = y - last_redraw_y;
  double dist = std::sqrt(dx*dx + dy*dy);
  
  if (dist > 5.0) {  // Only redraw after 5 pixels of movement
    gtk_widget_queue_draw(drawing_area_);
    last_redraw_x = x;
    last_redraw_y = y;
  }
}

bool MapView::handle_scroll(double, double dy)
{
  if(std::abs(dy) < 1e-6) {
    return false;
  }

  double old_zoom = zoom_;
  if(dy < 0) {
    zoom_ = std::min(zoom_ * kZoomStep, kMaxZoom);
  } else {
    zoom_ = std::max(zoom_ / kZoomStep, kMinZoom);
  }

  // Invalidate cache if zoom changed significantly
  if (std::abs(zoom_ - old_zoom) / old_zoom > 0.1) {
    viewport_cache_.invalidate();
  }

  gtk_widget_queue_draw(drawing_area_);
  return true;
}

bool MapView::handle_key_press(guint keyval, GdkModifierType state)
{
  bool handled = false;
  double step = kPanStep;
  if((state & GDK_SHIFT_MASK) != 0) {
    step *= 2.0;
  }

  switch(keyval) {
  case GDK_KEY_Up:
  case GDK_KEY_k:
    offset_y_ += step;
    handled = true;
    break;
  case GDK_KEY_Down:
  case GDK_KEY_j:
    offset_y_ -= step;
    handled = true;
    break;
  case GDK_KEY_Left:
  case GDK_KEY_h:
    offset_x_ += step;
    handled = true;
    break;
  case GDK_KEY_Right:
  case GDK_KEY_l:
    offset_x_ -= step;
    handled = true;
    break;
  case GDK_KEY_plus:
  case GDK_KEY_equal:
  case GDK_KEY_KP_Add:
    zoom_ = std::min(zoom_ * kZoomStep, kMaxZoom);
    handled = true;
    break;
  case GDK_KEY_minus:
  case GDK_KEY_KP_Subtract:
    zoom_ = std::max(zoom_ / kZoomStep, kMinZoom);
    handled = true;
    break;
  default:
    break;
  }

  if(handled) {
    gtk_widget_queue_draw(drawing_area_);
  }
  return handled;
}

void MapView::draw_cb(GtkDrawingArea *, cairo_t *cr, int width, int height, gpointer user_data)
{
  auto *self = static_cast<MapView *>(user_data);
  self->draw(cr, width, height);
}

void MapView::drag_begin_cb(GtkGestureDrag *, double start_x, double start_y, gpointer user_data)
{
  auto *self = static_cast<MapView *>(user_data);
  self->begin_drag(start_x, start_y);
}

void MapView::drag_update_cb(GtkGestureDrag *, double offset_x, double offset_y, gpointer user_data)
{
  auto *self = static_cast<MapView *>(user_data);
  self->update_drag(offset_x, offset_y);
}

gboolean MapView::scroll_cb(GtkEventControllerScroll *, double dx, double dy, gpointer user_data)
{
  auto *self = static_cast<MapView *>(user_data);
  return self->handle_scroll(dx, dy) ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
}

gboolean MapView::key_press_cb(GtkEventControllerKey *, guint keyval, guint, GdkModifierType state, gpointer user_data)
{
  auto *self = static_cast<MapView *>(user_data);
  return self->handle_key_press(keyval, state) ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
}

void MapView::ensure_coordinate_system()
{
  if (!renderer_) {
    renderer_ = std::make_unique<gisevo::rendering::Renderer>();
  }

  if (!map_data_ || !map_data_->load_success()) {
    coordinate_system_.reset();
    coordinate_system_initialized_ = false;
    if (renderer_) {
      renderer_->set_coordinate_system(nullptr);
    }
    return;
  }

  const auto current_bounds = map_data_->bounds();

  if (!coordinate_system_initialized_ || !bounds_equal(current_bounds, coordinate_system_bounds_)) {
    coordinate_system_ = std::make_shared<gisevo::rendering::CoordinateSystem>(current_bounds);
    renderer_->set_coordinate_system(coordinate_system_);
    coordinate_system_bounds_ = current_bounds;
    coordinate_system_initialized_ = true;
    viewport_cache_.invalidate();
  }
}

bool MapView::bounds_equal(const gisevo::core::Bounds &lhs, const gisevo::core::Bounds &rhs, double epsilon)
{
  return std::abs(lhs.min_lat - rhs.min_lat) <= epsilon &&
         std::abs(lhs.max_lat - rhs.max_lat) <= epsilon &&
         std::abs(lhs.min_lon - rhs.min_lon) <= epsilon &&
         std::abs(lhs.max_lon - rhs.max_lon) <= epsilon;
}

// Coordinate conversion functions
gisevo::core::Point2D MapView::latlon_to_screen(gisevo::core::LatLon latlon) const
{
  if (coordinate_system_) {
    return coordinate_system_->latlon_to_screen(latlon);
  }

  const double kDegreeToRadian = 0.017453292519943295; // PI / 180
  const double kEarthRadiusInMeters = 6371000.0;

  const auto bounds = map_data_ ? map_data_->bounds() : gisevo::core::Bounds{};
  const double center_lat = (bounds.min_lat + bounds.max_lat) * 0.5;
  const double center_lon = (bounds.min_lon + bounds.max_lon) * 0.5;

  double lat_rad = kDegreeToRadian * (latlon.latitude() - center_lat);
  double lon_rad = kDegreeToRadian * (latlon.longitude() - center_lon);
  double map_lat_avg_rad = map_data_ ? map_data_->average_latitude_rad() : 0.0;

  double x = kEarthRadiusInMeters * lon_rad * cos(map_lat_avg_rad);
  double y = kEarthRadiusInMeters * lat_rad;

  return gisevo::core::Point2D{x, y};
}

LatLon MapView::screen_to_latlon(double x, double y) const
{
  if (coordinate_system_) {
    return coordinate_system_->screen_to_latlon(gisevo::core::Point2D{x, y});
  }

  const double kDegreeToRadian = 0.017453292519943295; // PI / 180
  const double kEarthRadiusInMeters = 6371000.0;
  double map_lat_avg_rad = map_data_ ? map_data_->average_latitude_rad() : 0.0;

  const auto bounds = map_data_ ? map_data_->bounds() : gisevo::core::Bounds{};
  const double center_lat = (bounds.min_lat + bounds.max_lat) * 0.5;
  const double center_lon = (bounds.min_lon + bounds.max_lon) * 0.5;

  double map_x = x;
  double map_y = y;

  double lat_rad = map_y / kEarthRadiusInMeters;
  double lon_rad = map_x / (kEarthRadiusInMeters * cos(map_lat_avg_rad));

  double lat = center_lat + (lat_rad / kDegreeToRadian);
  double lon = center_lon + (lon_rad / kDegreeToRadian);

  return LatLon(lat, lon);
}

MapView::ViewportBounds MapView::get_viewport_bounds(int width, int height, double final_scale,
                                                    const gisevo::core::Point2D &map_center_screen) const
{
  ViewportBounds bounds{};

  if (!map_data_ || width <= 0 || height <= 0 || final_scale <= 0.0) {
    bounds.min_lat = bounds.max_lat = bounds.min_lon = bounds.max_lon = 0.0;
    return bounds;
  }

  const auto pixel_to_latlon = [&](double px, double py) {
    double map_x = ((px - width / 2.0 - offset_x_) / final_scale) + map_center_screen.x;
    double map_y = ((py - height / 2.0 - offset_y_) / final_scale) + map_center_screen.y;
    return screen_to_latlon(map_x, map_y);
  };

  const LatLon top_left = pixel_to_latlon(0.0, 0.0);
  const LatLon top_right = pixel_to_latlon(static_cast<double>(width), 0.0);
  const LatLon bottom_left = pixel_to_latlon(0.0, static_cast<double>(height));
  const LatLon bottom_right = pixel_to_latlon(static_cast<double>(width), static_cast<double>(height));

  double min_lat = std::min({top_left.latitude(), top_right.latitude(), bottom_left.latitude(), bottom_right.latitude()});
  double max_lat = std::max({top_left.latitude(), top_right.latitude(), bottom_left.latitude(), bottom_right.latitude()});
  double min_lon = std::min({top_left.longitude(), top_right.longitude(), bottom_left.longitude(), bottom_right.longitude()});
  double max_lon = std::max({top_left.longitude(), top_right.longitude(), bottom_left.longitude(), bottom_right.longitude()});

  const double lat_padding = std::max((max_lat - min_lat) * 0.05, 0.0005);
  const double lon_padding = std::max((max_lon - min_lon) * 0.05, 0.0005);

  bounds.min_lat = min_lat - lat_padding;
  bounds.max_lat = max_lat + lat_padding;
  bounds.min_lon = min_lon - lon_padding;
  bounds.max_lon = max_lon + lon_padding;

  return bounds;
}

bool MapView::is_visible_in_viewport(double x, double y, int width, int height) const
{
  // Check if point is visible in current viewport
  double screen_x = x * zoom_ + offset_x_ + width / 2.0;
  double screen_y = y * zoom_ + offset_y_ + height / 2.0;
  
  return screen_x >= -50 && screen_x <= width + 50 && 
         screen_y >= -50 && screen_y <= height + 50;
}

// Map rendering functions - simplified approach with existing coordinate system
void MapView::draw_features_by_type_simple(cairo_t *cr)
{
  cairo_save(cr);
  
  for (std::size_t feature_idx : viewport_cache_.cached_features) {
    const auto& feature = map_data_->feature(feature_idx);
    
    if (feature.points.size() < 3) {
      continue;
    }
    
    // Set color based on feature type
    switch (feature.type) {
      case FeatureType::PARK:
      case FeatureType::GREENSPACE:
      case FeatureType::GOLFCOURSE:
        cairo_set_source_rgba(cr, 0.0, 0.8, 0.0, 0.3);  // Green for parks
        break;
      case FeatureType::LAKE:
      case FeatureType::RIVER:
      case FeatureType::STREAM:
      case FeatureType::GLACIER:
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.8, 0.5);  // Blue for water
        break;
      case FeatureType::BUILDING:
        cairo_set_source_rgba(cr, 0.8, 0.0, 0.0, 0.4);  // Red for buildings
        break;
      default:
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.3);  // Gray for other
        break;
    }
    
    // Draw feature as polygon
    bool first_point = true;
    for (const auto& point : feature.points) {
      gisevo::core::Point2D screen_pos = latlon_to_screen(point);
      
      if (first_point) {
        cairo_move_to(cr, screen_pos.x, screen_pos.y);
        first_point = false;
      } else {
        cairo_line_to(cr, screen_pos.x, screen_pos.y);
      }
    }
    
    cairo_close_path(cr);
    cairo_fill(cr);
  }
  
  cairo_restore(cr);
}

void MapView::draw_streets_by_classification_simple(cairo_t *cr)
{
  // Classify streets by speed limit for different visual styles
  std::vector<std::size_t> highways, major_roads, minor_roads;
  
  for (std::size_t street_idx : viewport_cache_.cached_streets) {
    const auto& street = map_data_->street(street_idx);
    
    // Classify based on speed limit
    if (street.speed_limit_kph >= 80.0) {
      highways.push_back(street_idx);
    } else if (street.speed_limit_kph >= 50.0) {
      major_roads.push_back(street_idx);
    } else {
      minor_roads.push_back(street_idx);
    }
  }
  
  // Level-of-Detail rendering based on zoom level
  if (zoom_ < 0.5) {
    // Very low zoom: Only show highways
    draw_street_group(cr, highways, 3.0, 0.0, 0.0, 0.0);  // Black, thick
  } else if (zoom_ < 1.5) {
    // Low zoom: Show highways and major roads
    draw_street_group(cr, major_roads, 2.0, 0.2, 0.2, 0.2);  // Dark gray
    draw_street_group(cr, highways, 3.0, 0.0, 0.0, 0.0);     // Black, thick
  } else {
    // High zoom: Show all streets
    draw_street_group(cr, minor_roads, 1.5, 0.4, 0.4, 0.4);  // Light gray
    draw_street_group(cr, major_roads, 2.0, 0.2, 0.2, 0.2);  // Dark gray
    draw_street_group(cr, highways, 3.0, 0.0, 0.0, 0.0);     // Black, thick
  }
}

void MapView::draw_street_group(cairo_t *cr, const std::vector<std::size_t>& streets, 
                               double line_width, double r, double g, double b)
{
  if (streets.empty()) return;
  
  cairo_save(cr);
  cairo_set_line_width(cr, line_width / zoom_);
  cairo_set_source_rgb(cr, r, g, b);
  
  for (std::size_t street_idx : streets) {
    const auto& street = map_data_->street(street_idx);
    
    if (!street.curve_points.empty()) {
      // Draw with curve points
      gisevo::core::Point2D first_point = latlon_to_screen(street.curve_points[0]);
      cairo_move_to(cr, first_point.x, first_point.y);
      
      for (std::size_t i = 1; i < street.curve_points.size(); ++i) {
        gisevo::core::Point2D point = latlon_to_screen(street.curve_points[i]);
        cairo_line_to(cr, point.x, point.y);
      }
    } else {
      // Draw straight line between intersections
      gisevo::core::Point2D from_point = latlon_to_screen(street.from_position);
      gisevo::core::Point2D to_point = latlon_to_screen(street.to_position);
      cairo_move_to(cr, from_point.x, from_point.y);
      cairo_line_to(cr, to_point.x, to_point.y);
    }
  }
  
  cairo_stroke(cr);
  cairo_restore(cr);
}

void MapView::draw_intersections_simple(cairo_t *cr)
{
  cairo_save(cr);
  cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);  // Red for intersections
  
  for (std::size_t intersection_idx : viewport_cache_.cached_intersections) {
    const auto& intersection = map_data_->intersection(intersection_idx);
    gisevo::core::Point2D screen_pos = latlon_to_screen(intersection.position);
    
    cairo_arc(cr, screen_pos.x, screen_pos.y, 3.0 / zoom_, 0, 2 * M_PI);
    cairo_fill(cr);
  }
  
  cairo_restore(cr);
}

void MapView::draw_pois_simple(cairo_t *cr)
{
  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);  // Blue for POIs
  
  for (std::size_t poi_idx : viewport_cache_.cached_pois) {
    const auto& poi = map_data_->poi(poi_idx);
    gisevo::core::Point2D screen_pos = latlon_to_screen(poi.position);
    
    cairo_rectangle(cr, screen_pos.x - 2.0 / zoom_, screen_pos.y - 2.0 / zoom_, 
                    4.0 / zoom_, 4.0 / zoom_);
    cairo_fill(cr);
  }
  
  cairo_restore(cr);
}

// New rendering methods using the clean renderer architecture
void MapView::draw_features_with_renderer()
{
  if (!renderer_) {
    return;
  }
  
  // LOD: Filter features based on zoom level
  // zoom_ < 0.5: Very far out - only major water bodies
  // zoom_ < 1.5: Medium - add parks and large features  
  // zoom_ >= 1.5: Close - show all features including buildings
  
  std::vector<std::size_t> parks, water_features, buildings, other_features;
  
  for (std::size_t feature_idx : viewport_cache_.cached_features) {
    // Safety check: ensure feature index is valid
    if (feature_idx >= map_data_->feature_count()) {
      continue;
    }
    
    const auto& feature = map_data_->feature(feature_idx);
    
    if (feature.points.size() < 3) continue;
    
    // Calculate feature area for LOD decisions
    double area = calculate_feature_area(feature);
    
    switch (feature.type) {
      case FeatureType::LAKE:
      case FeatureType::RIVER:
        // Only show major water bodies at low zoom
        if (zoom_ < 0.8 || area > LARGE_FEATURE_THRESHOLD) {
          water_features.push_back(feature_idx);
        }
        break;
      case FeatureType::STREAM:
        // Only show streams at high zoom
        if (zoom_ >= 2.0) {
          water_features.push_back(feature_idx);
        }
        break;
      case FeatureType::GLACIER:
        // Show glaciers at medium+ zoom
        if (zoom_ >= 1.2) {
          water_features.push_back(feature_idx);
        }
        break;
      case FeatureType::PARK:
      case FeatureType::GREENSPACE:
      case FeatureType::GOLFCOURSE:
        // Show parks at high zoom only
        if (zoom_ >= 1.5) {
          parks.push_back(feature_idx);
        }
        break;
      case FeatureType::BUILDING:
        // Only show buildings at very high zoom
        if (zoom_ >= 3.0) {
          buildings.push_back(feature_idx);
        }
        break;
      default:
        // Unknown features only at very high zoom
        if (zoom_ >= 2.5) {
          other_features.push_back(feature_idx);
        }
        break;
    }
  }
  
  // Render filtered features with LOD
  renderer_->draw_features(water_features, *map_data_, gisevo::rendering::styles::feature_water);
  renderer_->draw_features(parks, *map_data_, gisevo::rendering::styles::feature_park);
  renderer_->draw_features(buildings, *map_data_, gisevo::rendering::styles::feature_building);
  
  gisevo::rendering::RenderStyle other_style;
  other_style.color = {0.5, 0.5, 0.5, 0.15};
  other_style.filled = true;
  other_style.stroked = true;
  renderer_->draw_features(other_features, *map_data_, other_style);
}

void MapView::draw_streets_with_renderer()
{
  if (!renderer_) {
    return;
  }
  // Classify streets by speed limit for different visual styles
  std::vector<std::size_t> highways, major_roads, minor_roads;
  
  for (std::size_t street_idx : viewport_cache_.cached_streets) {
    const auto& street = map_data_->street(street_idx);
    
    // Classify based on speed limit
    if (street.speed_limit_kph >= 80.0) {
      highways.push_back(street_idx);
    } else if (street.speed_limit_kph >= 50.0) {
      major_roads.push_back(street_idx);
    } else {
      minor_roads.push_back(street_idx);
    }
  }
  
  // Level-of-Detail rendering based on zoom level
  if (zoom_ < 0.5) {
    // Very low zoom: Only show highways
    renderer_->draw_streets(highways, *map_data_, gisevo::rendering::styles::street_highway);
  } else if (zoom_ < 1.5) {
    // Low zoom: Show highways and major roads
    gisevo::rendering::RenderStyle major_style;
    major_style.line_width = 2.5;
    major_style.color = {0.15, 0.15, 0.15, 1.0};
    major_style.stroked = true;
    
    renderer_->draw_streets(major_roads, *map_data_, major_style);
    renderer_->draw_streets(highways, *map_data_, gisevo::rendering::styles::street_highway);
  } else {
    // High zoom: Show all streets
    gisevo::rendering::RenderStyle minor_style;
    minor_style.line_width = 1.8;
    minor_style.color = {0.3, 0.3, 0.3, 1.0};
    minor_style.stroked = true;
    
    gisevo::rendering::RenderStyle major_style;
    major_style.line_width = 2.5;
    major_style.color = {0.15, 0.15, 0.15, 1.0};
    major_style.stroked = true;
    
    renderer_->draw_streets(minor_roads, *map_data_, minor_style);
    renderer_->draw_streets(major_roads, *map_data_, major_style);
    renderer_->draw_streets(highways, *map_data_, gisevo::rendering::styles::street_highway);
  }
}

void MapView::draw_intersections_with_renderer()
{
  if (!renderer_) {
    return;
  }
  renderer_->draw_intersections(viewport_cache_.cached_intersections, *map_data_, 
                               gisevo::rendering::styles::intersection_default);
}

void MapView::draw_pois_with_renderer()
{
  if (!renderer_) {
    return;
  }
  renderer_->draw_pois(viewport_cache_.cached_pois, *map_data_, 
                      gisevo::rendering::styles::poi_default);
}

double MapView::calculate_feature_area(const gisevo::core::Feature& feature) const {
  if (feature.points.size() < 3) return 0.0;
  
  // Safety check: ensure we have valid points
  if (feature.points.empty()) return 0.0;
  
  // Simple bounding box area calculation
  double min_lat = feature.points[0].lat;
  double max_lat = feature.points[0].lat;
  double min_lon = feature.points[0].lon;
  double max_lon = feature.points[0].lon;
  
  for (const auto& point : feature.points) {
    min_lat = std::min(min_lat, point.lat);
    max_lat = std::max(max_lat, point.lat);
    min_lon = std::min(min_lon, point.lon);
    max_lon = std::max(max_lon, point.lon);
  }
  
  return (max_lat - min_lat) * (max_lon - min_lon);
}
