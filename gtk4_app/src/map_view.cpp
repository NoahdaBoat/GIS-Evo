#include "map_view.hpp"

#include <algorithm>
#include <cmath>
#include <gdk/gdkkeysyms.h>
#include "StreetsDatabaseAPI.h"

namespace {
constexpr double kZoomStep = 1.1;
constexpr double kMinZoom = 0.25;
constexpr double kMaxZoom = 8.0;
constexpr double kPanStep = 32.0;
}

MapView::MapView(std::shared_ptr<gisevo::core::MapData> map_data)
    : drawing_area_(gtk_drawing_area_new())
    , map_data_(map_data)
    , offset_x_(0.0)
    , offset_y_(0.0)
    , zoom_(1.0)  // Start with normal zoom level for debugging
    , drag_start_x_(0.0)
    , drag_start_y_(0.0)
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
}

GtkWidget *MapView::widget() const
{
  return drawing_area_;
}

void MapView::draw(cairo_t *cr, int width, int height)
{
  cairo_save(cr);
  
  // Debug output
  static int draw_count = 0;
  if (draw_count++ % 60 == 0) {  // Every ~1 second at 60fps
  g_print("DEBUG: GTK4 draw() called, width=%d, height=%d, zoom=%.3f\n", width, height, zoom_);
  g_print("DEBUG: Global bounds check: min_lat=%.6f, max_lat=%.6f, min_lon=%.6f, max_lon=%.6f\n",
          map_data_->bounds().min_lat, map_data_->bounds().max_lat, map_data_->bounds().min_lon, map_data_->bounds().max_lon);
  }
  
  // Set background color
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);  // White background for better contrast
  cairo_paint(cr);

  // Simplified coordinate transformation:
  // 1. Calculate map center in lat/lon
  double map_center_lat = (map_data_->bounds().min_lat + map_data_->bounds().max_lat) / 2.0;
  double map_center_lon = (map_data_->bounds().min_lon + map_data_->bounds().max_lon) / 2.0;
  
  // 2. Convert map center to screen coordinates
  gisevo::core::Point2D map_center_screen = latlon_to_screen(gisevo::core::LatLon(map_center_lat, map_center_lon));
  
  // 3. Calculate scale to fit map in viewport (80% of window)
  gisevo::core::Point2D min_screen = latlon_to_screen(gisevo::core::LatLon(map_data_->bounds().min_lat, map_data_->bounds().min_lon));
  gisevo::core::Point2D max_screen = latlon_to_screen(gisevo::core::LatLon(map_data_->bounds().max_lat, map_data_->bounds().max_lon));
  double map_width = max_screen.x - min_screen.x;
  double map_height = max_screen.y - min_screen.y;
  double scale_x = (width * 0.8) / map_width;
  double scale_y = (height * 0.8) / map_height;
  double base_scale = std::min(scale_x, scale_y);
  double final_scale = base_scale * zoom_;
  
  // 4. Apply transformations in order:
  //    - Move to center of window
  //    - Apply pan offset
  //    - Scale to zoom level
  //    - Move to center map at origin
  cairo_translate(cr, width / 2.0 + offset_x_, height / 2.0 + offset_y_);
  cairo_scale(cr, final_scale, final_scale);
  cairo_translate(cr, -map_center_screen.x, -map_center_screen.y);
  
  // Debug: Print transformation info
  static int center_debug_count = 0;
  if (center_debug_count++ % 60 == 0) {
    g_print("DEBUG: Map center: lat=%.6f, lon=%.6f, screen=(%.1f,%.1f)\n", 
            map_center_lat, map_center_lon, map_center_screen.x, map_center_screen.y);
    g_print("DEBUG: Scale: base=%.6f, final=%.6f, zoom=%.3f\n", 
            base_scale, final_scale, zoom_);
  }

  // Debug: Print some actual feature coordinates after transformation
  static int coord_debug_count = 0;
  if (coord_debug_count++ % 60 == 0) {
    g_print("DEBUG: Map center screen coordinates: (%.1f, %.1f)\n", map_center_screen.x, map_center_screen.y);
    g_print("DEBUG: Current transformation: translate(%.1f, %.1f), scale(%.6f)\n", 
            -map_center_screen.x, -map_center_screen.y, final_scale);
  }
  
  // Draw map elements
  draw_features(cr, width, height);
  draw_streets(cr, width, height);
  draw_intersections(cr, width, height);
  draw_pois(cr, width, height);

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

// Coordinate conversion functions
gisevo::core::Point2D MapView::latlon_to_screen(gisevo::core::LatLon latlon) const
{
  // Use proper geographic projection like the legacy system
  // This matches the convertLatLonToXY function from ms1helpers.cpp
  
  // Constants from m1.h
  const double kDegreeToRadian = 0.017453292519943295; // PI / 180
  const double kEarthRadiusInMeters = 6371000.0;
  
  // Convert lat/lon to radians
  double lat_rad = kDegreeToRadian * latlon.latitude();
  double lon_rad = kDegreeToRadian * latlon.longitude();
  
  // Use proper geographic projection with map's average latitude
  double map_lat_avg_rad = map_data_->average_latitude_rad();
  
  double x = kEarthRadiusInMeters * lon_rad * cos(map_lat_avg_rad);
  double y = kEarthRadiusInMeters * lat_rad;
  
  return gisevo::core::Point2D{x, y};
}

LatLon MapView::screen_to_latlon(double x, double y) const
{
  // Convert screen coordinates to lat/lon accounting for map transformation
  const double kDegreeToRadian = 0.017453292519943295; // PI / 180
  const double kEarthRadiusInMeters = 6371000.0;
  double map_lat_avg_rad = map_data_->average_latitude_rad();
  
  // Apply inverse transformation to get coordinates in map space
  // First, account for the map center translation
  gisevo::core::Point2D map_center_screen = latlon_to_screen(gisevo::core::LatLon(
    (map_data_->bounds().min_lat + map_data_->bounds().max_lat) / 2.0,
    (map_data_->bounds().min_lon + map_data_->bounds().max_lon) / 2.0
  ));
  
  // Convert screen coordinates to map coordinates
  double map_x = x + map_center_screen.x;
  double map_y = y + map_center_screen.y;
  
  // Convert map coordinates to lat/lon
  double lat_rad = map_y / kEarthRadiusInMeters;
  double lon_rad = map_x / (kEarthRadiusInMeters * cos(map_lat_avg_rad));
  
  double lat = lat_rad / kDegreeToRadian;
  double lon = lon_rad / kDegreeToRadian;
  
  return LatLon(lat, lon);
}

MapView::ViewportBounds MapView::get_viewport_bounds(int width, int height) const
{
  // Calculate viewport bounds based on current map transformation
  // Get the map center in lat/lon
  double map_center_lat = (map_data_->bounds().min_lat + map_data_->bounds().max_lat) / 2.0;
  double map_center_lon = (map_data_->bounds().min_lon + map_data_->bounds().max_lon) / 2.0;
  
  // Calculate the scale factor used in the draw function
  gisevo::core::Point2D min_screen = latlon_to_screen(gisevo::core::LatLon(map_data_->bounds().min_lat, map_data_->bounds().min_lon));
  gisevo::core::Point2D max_screen = latlon_to_screen(gisevo::core::LatLon(map_data_->bounds().max_lat, map_data_->bounds().max_lon));
  double map_width = max_screen.x - min_screen.x;
  double map_height = max_screen.y - min_screen.y;
  double scale_x = (width * 0.8) / map_width;
  double scale_y = (height * 0.8) / map_height;
  double scale = std::min(scale_x, scale_y) * zoom_;
  
  // Calculate viewport size in map coordinates
  double viewport_width_map = width / scale;
  double viewport_height_map = height / scale;
  
  // Convert viewport size to lat/lon degrees
  const double kDegreeToRadian = 0.017453292519943295;
  const double kEarthRadiusInMeters = 6371000.0;
  double map_lat_avg_rad = map_data_->average_latitude_rad();
  
  double lat_range = viewport_height_map / kEarthRadiusInMeters / kDegreeToRadian;
  double lon_range = viewport_width_map / (kEarthRadiusInMeters * cos(map_lat_avg_rad)) / kDegreeToRadian;
  
  // Calculate bounds around map center with full range
  double padding = 0.001; // Very small padding
  ViewportBounds bounds = {
    map_center_lon - lon_range/2 - padding,  // Use /2 for full viewport
    map_center_lat - lat_range/2 - padding,
    map_center_lon + lon_range/2 + padding,
    map_center_lat + lat_range/2 + padding
  };
  
  // Debug output
  static int bounds_debug_count = 0;
  if (bounds_debug_count++ % 60 == 0) {  // Every ~1 second at 60fps
    g_print("DEBUG: Viewport bounds: min_lon=%.6f, min_lat=%.6f, max_lon=%.6f, max_lat=%.6f\n", 
            bounds.min_lon, bounds.min_lat, bounds.max_lon, bounds.max_lat);
    g_print("DEBUG: Map center: lat=%.6f, lon=%.6f, scale=%.6f\n", 
            map_center_lat, map_center_lon, scale);
  }
  
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

// Map rendering functions
void MapView::draw_streets(cairo_t *cr, int width, int height)
{
  cairo_set_line_width(cr, 2.0 / zoom_);  // Thicker lines
  cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Darker gray for streets
  
  // Get viewport bounds for spatial query
  ViewportBounds bounds = get_viewport_bounds(width, height);
  
  // Use cached data if available, otherwise query and cache
  viewport_cache_.ensure(bounds, *map_data_);
  const auto& visible_streets = viewport_cache_.cached_streets;
  
  // Debug output
  static int street_debug_count = 0;
  if (street_debug_count++ % 60 == 0) {  // Every ~1 second at 60fps
    g_print("DEBUG: draw_streets called, visible_streets=%zu, zoom=%.6f\n", 
            visible_streets.size(), zoom_);
  }
  
  int streets_drawn = 0;
  for (std::size_t segment_idx : visible_streets) {
    const auto& segment = map_data_->street(segment_idx);
    
    gisevo::core::Point2D from_screen = latlon_to_screen(segment.from_position);
    gisevo::core::Point2D to_screen = latlon_to_screen(segment.to_position);
    
    streets_drawn++;
    
    // Draw street segment with curve points if they exist
    if (!segment.curve_points.empty()) {
      // Draw from intersection to first curve point
      const auto& first_curve = segment.curve_points.front();
      gisevo::core::Point2D first_curve_screen = latlon_to_screen(first_curve);
      
      cairo_move_to(cr, from_screen.x, from_screen.y);
      cairo_line_to(cr, first_curve_screen.x, first_curve_screen.y);
      
      // Draw between curve points
      for (std::size_t j = 0; j + 1 < segment.curve_points.size(); ++j) {
        gisevo::core::Point2D curve1_screen = latlon_to_screen(segment.curve_points[j]);
        gisevo::core::Point2D curve2_screen = latlon_to_screen(segment.curve_points[j + 1]);
        
        cairo_move_to(cr, curve1_screen.x, curve1_screen.y);
        cairo_line_to(cr, curve2_screen.x, curve2_screen.y);
      }
      
      // Draw from last curve point to intersection
      gisevo::core::Point2D last_curve_screen = latlon_to_screen(segment.curve_points.back());
      
      cairo_move_to(cr, last_curve_screen.x, last_curve_screen.y);
      cairo_line_to(cr, to_screen.x, to_screen.y);
    } else {
      // No curve points - draw straight line between intersections
      cairo_move_to(cr, from_screen.x, from_screen.y);
      cairo_line_to(cr, to_screen.x, to_screen.y);
    }
  }
  
  if (street_debug_count % 60 == 0) {
    g_print("DEBUG: Drew %d streets out of %zu queried\n", streets_drawn, visible_streets.size());
  }
  
  cairo_stroke(cr);
}

void MapView::draw_intersections(cairo_t *cr, int width, int height)
{
  cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);  // Bright red for intersections
  
  // Get viewport bounds for spatial query
  ViewportBounds bounds = get_viewport_bounds(width, height);
  
  // Use cached data if available, otherwise query and cache
  viewport_cache_.ensure(bounds, *map_data_);
  const auto& visible_intersections = viewport_cache_.cached_intersections;
  
  for (std::size_t intersection_idx : visible_intersections) {
    const auto& intersection = map_data_->intersection(intersection_idx);
    gisevo::core::Point2D screen_pos = latlon_to_screen(intersection.position);
    
    cairo_arc(cr, screen_pos.x, screen_pos.y, 3.0 / zoom_, 0, 2 * M_PI);  // Larger dots
    cairo_fill(cr);
  }
}

void MapView::draw_pois(cairo_t *cr, int width, int height)
{
  cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);  // Bright blue for POIs
  
  // Get viewport bounds for spatial query
  ViewportBounds bounds = get_viewport_bounds(width, height);
  
  // Use cached data if available, otherwise query and cache
  viewport_cache_.ensure(bounds, *map_data_);
  const auto& visible_pois = viewport_cache_.cached_pois;
  
  for (std::size_t poi_idx : visible_pois) {
    const auto& poi = map_data_->poi(poi_idx);
    gisevo::core::Point2D screen_pos = latlon_to_screen(poi.position);
    
    cairo_rectangle(cr, screen_pos.x - 2.0 / zoom_, screen_pos.y - 2.0 / zoom_, 
                    4.0 / zoom_, 4.0 / zoom_);  // Larger squares
    cairo_fill(cr);
  }
}

void MapView::draw_features(cairo_t *cr, int width, int height)
{
  cairo_save(cr);
  
  // Get viewport bounds for spatial query
  ViewportBounds bounds = get_viewport_bounds(width, height);
  
  // Use cached data if available, otherwise query and cache
  viewport_cache_.ensure(bounds, *map_data_);
  const auto& visible_features = viewport_cache_.cached_features;
  
  // Debug output
  static int feature_debug_count = 0;
  if (feature_debug_count++ % 60 == 0) {  // Every ~1 second at 60fps
    g_print("DEBUG: GTK4 draw_features called, visible_features=%zu, zoom=%.3f\n", 
            visible_features.size(), zoom_);
  }
  
  for (std::size_t feature_idx : visible_features) {
    const auto& feature = map_data_->feature(feature_idx);
    
    // Skip unknown features
    if (feature.type == FeatureType::UNKNOWN) {
      continue;
    }
    
    if (feature.points.size() < 3) {
      continue;
    }
    
    // Set color based on feature type - use very distinct colors
    switch (feature.type) {
      case FeatureType::PARK:
      case FeatureType::GREENSPACE:
        cairo_set_source_rgb(cr, 0.0, 0.8, 0.0);  // Dark green for parks
        break;
      case FeatureType::LAKE:
      case FeatureType::RIVER:
      case FeatureType::STREAM:
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.8);  // Dark blue for water
        break;
      case FeatureType::BUILDING:
        cairo_set_source_rgb(cr, 0.8, 0.0, 0.0);  // Dark red for buildings
        break;
      case FeatureType::BEACH:
        cairo_set_source_rgb(cr, 0.8, 0.8, 0.0);  // Dark yellow for beaches
        break;
      case FeatureType::ISLAND:
        cairo_set_source_rgb(cr, 0.6, 0.3, 0.0);  // Brown for islands
        break;
      case FeatureType::GOLFCOURSE:
        cairo_set_source_rgb(cr, 0.0, 0.6, 0.0);  // Medium green for golf courses
        break;
      case FeatureType::GLACIER:
        cairo_set_source_rgb(cr, 0.6, 0.6, 0.8);  // Light blue for glaciers
        break;
      case FeatureType::UNKNOWN:
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);  // Gray for unknown
        break;
      default:
        cairo_set_source_rgb(cr, 0.8, 0.0, 0.8);  // Purple for other features
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
    
    // Close the polygon and fill it
    cairo_close_path(cr);
    cairo_fill(cr);
  }
  
  cairo_restore(cr);
}
