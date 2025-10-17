#pragma once

#include "map_data.hpp"
#include "renderer.hpp"
#include "types.hpp"
#include <gtk/gtk.h>
#include <memory>
#include <unordered_map>

namespace gisevo::ui {

// Clean viewport management
class Viewport {
public:
    Viewport();
    
    // Viewport state
    void set_size(int width, int height);
    void set_zoom(double zoom);
    void set_offset(const gisevo::core::Point2D& offset);
    
    // Viewport queries
    gisevo::core::Bounds visible_bounds() const;
    bool is_visible(const gisevo::core::LatLon& point) const;
    
    // Pan and zoom operations
    void pan_by(const gisevo::core::Point2D& delta);
    void zoom_to(double zoom, const gisevo::core::Point2D& center);
    void fit_to_bounds(const gisevo::core::Bounds& bounds);
    
    // Getters
    int width() const { return width_; }
    int height() const { return height_; }
    double zoom() const { return zoom_; }
    const gisevo::core::Point2D& offset() const { return offset_; }

private:
    int width_ = 0;
    int height_ = 0;
    double zoom_ = 1.0;
    gisevo::core::Point2D offset_{0, 0};
    
    std::shared_ptr<gisevo::rendering::CoordinateSystem> coords_;
    
    void update_coordinate_system();
};

// Clean map view widget
class MapView {
public:
    MapView();
    ~MapView();
    
    // GTK4 integration
    GtkWidget* widget() const;
    
    // Map data management
    void set_map_data(std::shared_ptr<gisevo::core::MapData> data);
    std::shared_ptr<gisevo::core::MapData> map_data() const { return map_data_; }
    
    // Viewport management
    Viewport& viewport() { return viewport_; }
    const Viewport& viewport() const { return viewport_; }
    
    // Rendering control
    void queue_redraw();
    void set_render_styles(const std::unordered_map<std::string, gisevo::rendering::RenderStyle>& styles);

private:
    // GTK4 widgets
    GtkWidget* drawing_area_;
    
    // Core components
    std::shared_ptr<gisevo::core::MapData> map_data_;
    std::unique_ptr<gisevo::rendering::Renderer> renderer_;
    Viewport viewport_;
    
    // Interaction state
    gisevo::core::Point2D drag_start_;
    bool is_dragging_ = false;
    
    // Rendering styles
    std::unordered_map<std::string, gisevo::rendering::RenderStyle> styles_;
    
    // GTK4 callbacks
    static void draw_callback(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
    static void drag_begin_callback(GtkGestureDrag* gesture, double start_x, double start_y, gpointer user_data);
    static void drag_update_callback(GtkGestureDrag* gesture, double offset_x, double offset_y, gpointer user_data);
    static gboolean scroll_callback(GtkEventControllerScroll* controller, double dx, double dy, gpointer user_data);
    static gboolean key_press_callback(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
    
    // Internal methods
    void draw(cairo_t* cr, int width, int height);
    void begin_drag(double x, double y);
    void update_drag(double x, double y);
    bool handle_scroll(double dx, double dy);
    bool handle_key_press(guint keyval, GdkModifierType state);
    
    void setup_interactions();
    void setup_default_styles();
};

// Clean application class
class Application {
public:
    Application();
    ~Application();
    
    // Application lifecycle
    int run(int argc, char* argv[]);
    void quit();
    
    // Map management
    bool load_map(const std::string& streets_path, const std::string& osm_path);
    void create_main_window();
    
    // Getters
    MapView* map_view() const { return map_view_.get(); }

private:
    GtkApplication* app_;
    GtkWidget* main_window_;
    std::unique_ptr<MapView> map_view_;
    std::shared_ptr<gisevo::core::MapData> map_data_;
    
    // GTK4 callbacks
    static void on_activate(GtkApplication* app, gpointer user_data);
    static void on_window_close(GtkWidget* window, gpointer user_data);
    
    // Internal methods
    void setup_application();
    void create_window();
};

} // namespace gisevo::ui
