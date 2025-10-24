#pragma once

#include <gtk/gtk.h>
#include <vector>
#include <cmath>
#include <memory>
#include "core/map_data.hpp"
#include "core/renderer.hpp"

class MapView {
public:
  MapView(std::shared_ptr<gisevo::core::MapData> map_data);
  MapView(const MapView &) = delete;
  MapView &operator=(const MapView &) = delete;
  MapView(MapView &&) = delete;
  MapView &operator=(MapView &&) = delete;
  ~MapView() = default;

  GtkWidget *widget() const;
  
  // POI visibility control
  void set_show_pois(bool show);

private:
  // Viewport bounds for spatial queries
  struct ViewportBounds {
    double min_lon, min_lat, max_lon, max_lat;
  };
  
  // Viewport cache for performance optimization
  struct ViewportCache {
    ViewportBounds cached_bounds;
    std::vector<std::size_t> cached_streets;
    std::vector<std::size_t> cached_intersections;
    std::vector<std::size_t> cached_pois;
    std::vector<std::size_t> cached_features;
    bool is_valid = false;
    
    bool should_invalidate(const ViewportBounds& new_bounds, double zoom_change_threshold = 0.1);
    void update(const ViewportBounds& bounds, const gisevo::core::MapData& map_data);
    void invalidate();
    void ensure(const ViewportBounds& bounds, const gisevo::core::MapData& map_data);
  };
  
  GtkWidget *drawing_area_;
  std::shared_ptr<gisevo::core::MapData> map_data_;
  double offset_x_;
  double offset_y_;
  double zoom_;
  double drag_start_x_;
  double drag_start_y_;
  
  // Rendering system
  std::unique_ptr<gisevo::rendering::Renderer> renderer_;
  std::shared_ptr<gisevo::rendering::CoordinateSystem> coordinate_system_;
  
  // Viewport cache for performance
  ViewportCache viewport_cache_;
  gisevo::core::Bounds coordinate_system_bounds_{};
  bool coordinate_system_initialized_ = false;
  
  // POI visibility control
  bool show_pois_ = true;
  
  // Coordinate conversion functions
  gisevo::core::Point2D latlon_to_screen(gisevo::core::LatLon latlon) const;
  gisevo::core::LatLon screen_to_latlon(double x, double y) const;
  ViewportBounds get_viewport_bounds(int width, int height, double final_scale,
                                     const gisevo::core::Point2D &map_center_screen) const;
  
  // Feature analysis functions
  double calculate_feature_area(const gisevo::core::Feature& feature) const;

  void ensure_coordinate_system();
  static bool bounds_equal(const gisevo::core::Bounds &lhs, const gisevo::core::Bounds &rhs, double epsilon = 1e-6);
  
  // Map rendering functions using the clean renderer architecture
  void draw_features_with_renderer();
  void draw_streets_with_renderer();
  void draw_intersections_with_renderer();
  void draw_pois_with_renderer();
  
  // Legacy rendering functions (kept for reference, can be removed later)
  void draw_features_by_type_simple(cairo_t *cr);
  void draw_streets_by_classification_simple(cairo_t *cr);
  void draw_street_group(cairo_t *cr, const std::vector<std::size_t>& streets, 
                        double line_width, double r, double g, double b);
  void draw_intersections_simple(cairo_t *cr);
  void draw_pois_simple(cairo_t *cr);
  bool is_visible_in_viewport(double x, double y, int width, int height) const;

  void draw(cairo_t *cr, int width, int height);
  void begin_drag(double x, double y);
  void update_drag(double x, double y);
  bool handle_scroll(double dx, double dy);
  bool handle_key_press(guint keyval, GdkModifierType state);

  static void draw_cb(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
  static void drag_begin_cb(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data);
  static void drag_update_cb(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data);
  static gboolean scroll_cb(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data);
  static gboolean key_press_cb(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
};
