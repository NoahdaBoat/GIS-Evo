#include "ui.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace gisevo::ui {

// Viewport implementation
Viewport::Viewport() {
    update_coordinate_system();
}

void Viewport::set_size(int width, int height) {
    width_ = width;
    height_ = height;
    update_coordinate_system();
}

void Viewport::set_zoom(double zoom) {
    zoom_ = std::max(0.1, std::min(10.0, zoom));
    update_coordinate_system();
}

void Viewport::set_offset(const gisevo::core::Point2D& offset) {
    offset_ = offset;
}

gisevo::core::Bounds Viewport::visible_bounds() const {
    if (!coords_) {
        return gisevo::core::Bounds{0, 0, 0, 0};
    }
    
    // Convert viewport corners to lat/lon
    gisevo::core::Point2D top_left(-offset_.x, -offset_.y);
    gisevo::core::Point2D bottom_right(width_ - offset_.x, height_ - offset_.y);
    
    gisevo::core::LatLon top_left_ll = coords_->screen_to_latlon(top_left);
    gisevo::core::LatLon bottom_right_ll = coords_->screen_to_latlon(bottom_right);
    
    return gisevo::core::Bounds{
        std::min(top_left_ll.lat, bottom_right_ll.lat),
        std::max(top_left_ll.lat, bottom_right_ll.lat),
        std::min(top_left_ll.lon, bottom_right_ll.lon),
        std::max(top_left_ll.lon, bottom_right_ll.lon)
    };
}

bool Viewport::is_visible(const gisevo::core::LatLon& point) const {
    return visible_bounds().contains(point);
}

void Viewport::pan_by(const gisevo::core::Point2D& delta) {
    offset_.x += delta.x;
    offset_.y += delta.y;
}

void Viewport::zoom_to(double zoom, [[maybe_unused]] const gisevo::core::Point2D& center) {
    // TODO: Implement zoom to center
    set_zoom(zoom);
}

void Viewport::fit_to_bounds(const gisevo::core::Bounds& bounds) {
    if (!coords_) return;
    
    // Calculate scale to fit bounds in viewport
    double scale = coords_->calculate_scale(width_, height_, 1.0);
    
    // Center the bounds
    gisevo::core::LatLon center = bounds.center();
    gisevo::core::Point2D center_screen = coords_->latlon_to_screen(center);
    
    offset_.x = width_ / 2.0 - center_screen.x * scale;
    offset_.y = height_ / 2.0 - center_screen.y * scale;
    
    set_zoom(scale);
}

void Viewport::update_coordinate_system() {
    // This will be set when map data is loaded
    // For now, create a dummy coordinate system
    gisevo::core::Bounds dummy_bounds{-180, 180, -90, 90};
    coords_ = std::make_shared<gisevo::rendering::CoordinateSystem>(dummy_bounds);
}

// MapView implementation
MapView::MapView() {
    drawing_area_ = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawing_area_, TRUE);
    gtk_widget_set_vexpand(drawing_area_, TRUE);
    
    renderer_ = std::make_unique<gisevo::rendering::Renderer>();
    
    setup_interactions();
    setup_default_styles();
}

MapView::~MapView() = default;

GtkWidget* MapView::widget() const {
    return drawing_area_;
}

void MapView::set_map_data(std::shared_ptr<gisevo::core::MapData> data) {
    map_data_ = data;
    
    if (data) {
        // Update coordinate system with map bounds
        auto coords = std::make_shared<gisevo::rendering::CoordinateSystem>(data->bounds());
        renderer_->set_coordinate_system(coords);
        
        // Fit viewport to map bounds
        viewport_.fit_to_bounds(data->bounds());
    }
    
    queue_redraw();
}

void MapView::queue_redraw() {
    gtk_widget_queue_draw(drawing_area_);
}

void MapView::set_render_styles(const std::unordered_map<std::string, gisevo::rendering::RenderStyle>& styles) {
    styles_ = styles;
    queue_redraw();
}

void MapView::draw(cairo_t* cr, int width, int height) {
    if (!map_data_) return;
    
    viewport_.set_size(width, height);
    
    // Begin rendering frame
    renderer_->begin_frame(cr, width, height, viewport_.zoom(), viewport_.offset());
    
    // Get visible elements
    auto visible_bounds = viewport_.visible_bounds();
    auto visible_streets = map_data_->streets_in_bounds(visible_bounds);
    auto visible_intersections = map_data_->intersections_in_bounds(visible_bounds);
    auto visible_pois = map_data_->pois_in_bounds(visible_bounds);
    auto visible_features = map_data_->features_in_bounds(visible_bounds);
    
    // Render layers
    renderer_->draw_features(visible_features, *map_data_, styles_["feature_default"]);
    renderer_->draw_streets(visible_streets, *map_data_, styles_["street_default"]);
    renderer_->draw_intersections(visible_intersections, *map_data_, styles_["intersection_default"]);
    renderer_->draw_pois(visible_pois, *map_data_, styles_["poi_default"]);
    
    // End rendering frame
    renderer_->end_frame();
}

void MapView::begin_drag(double x, double y) {
    is_dragging_ = true;
    drag_start_ = gisevo::core::Point2D(x, y);
}

void MapView::update_drag(double x, double y) {
    if (!is_dragging_) return;
    
    gisevo::core::Point2D delta(x - drag_start_.x, y - drag_start_.y);
    viewport_.pan_by(delta);
    drag_start_ = gisevo::core::Point2D(x, y);
    
    queue_redraw();
}

bool MapView::handle_scroll([[maybe_unused]] double dx, double dy) {
    if (std::abs(dy) < 1e-6) return false;
    
    double old_zoom = viewport_.zoom();
    double new_zoom = dy < 0 ? old_zoom * 1.1 : old_zoom / 1.1;
    new_zoom = std::max(0.1, std::min(10.0, new_zoom));
    
    viewport_.set_zoom(new_zoom);
    queue_redraw();
    return true;
}

bool MapView::handle_key_press(guint keyval, GdkModifierType state) {
    bool handled = false;
    double step = 32.0;
    
    if (state & GDK_SHIFT_MASK) {
        step *= 2.0;
    }
    
    switch (keyval) {
    case GDK_KEY_Up:
    case GDK_KEY_k:
        viewport_.pan_by(gisevo::core::Point2D(0, step));
        handled = true;
        break;
    case GDK_KEY_Down:
    case GDK_KEY_j:
        viewport_.pan_by(gisevo::core::Point2D(0, -step));
        handled = true;
        break;
    case GDK_KEY_Left:
    case GDK_KEY_h:
        viewport_.pan_by(gisevo::core::Point2D(step, 0));
        handled = true;
        break;
    case GDK_KEY_Right:
    case GDK_KEY_l:
        viewport_.pan_by(gisevo::core::Point2D(-step, 0));
        handled = true;
        break;
    case GDK_KEY_plus:
    case GDK_KEY_equal:
        viewport_.set_zoom(std::min(viewport_.zoom() * 1.1, 10.0));
        handled = true;
        break;
    case GDK_KEY_minus:
        viewport_.set_zoom(std::max(viewport_.zoom() / 1.1, 0.1));
        handled = true;
        break;
    default:
        break;
    }
    
    if (handled) {
        queue_redraw();
    }
    return handled;
}

void MapView::setup_interactions() {
    // Drawing callback
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area_), draw_callback, this, nullptr);
    
    // Drag gesture
    GtkGesture* drag = gtk_gesture_drag_new();
    gtk_widget_add_controller(drawing_area_, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin_callback), this);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update_callback), this);
    
    // Scroll controller
    GtkEventController* scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    gtk_widget_add_controller(drawing_area_, scroll);
    g_signal_connect(scroll, "scroll", G_CALLBACK(scroll_callback), this);
    
    // Keyboard controller
    GtkEventController* key = gtk_event_controller_key_new();
    gtk_widget_add_controller(drawing_area_, key);
    g_signal_connect(key, "key-pressed", G_CALLBACK(key_press_callback), this);
}

void MapView::setup_default_styles() {
    // Set up default rendering styles
    styles_["street_default"] = gisevo::rendering::styles::street_default;
    styles_["intersection_default"] = gisevo::rendering::styles::intersection_default;
    styles_["poi_default"] = gisevo::rendering::styles::poi_default;
    styles_["feature_default"] = gisevo::rendering::styles::feature_park;
}

// Static callback implementations
void MapView::draw_callback([[maybe_unused]] GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data) {
    auto* self = static_cast<MapView*>(user_data);
    self->draw(cr, width, height);
}

void MapView::drag_begin_callback([[maybe_unused]] GtkGestureDrag* gesture, double start_x, double start_y, gpointer user_data) {
    auto* self = static_cast<MapView*>(user_data);
    self->begin_drag(start_x, start_y);
}

void MapView::drag_update_callback([[maybe_unused]] GtkGestureDrag* gesture, double offset_x, double offset_y, gpointer user_data) {
    auto* self = static_cast<MapView*>(user_data);
    self->update_drag(offset_x, offset_y);
}

gboolean MapView::scroll_callback([[maybe_unused]] GtkEventControllerScroll* controller, double dx, double dy, gpointer user_data) {
    auto* self = static_cast<MapView*>(user_data);
    return self->handle_scroll(dx, dy) ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
}

gboolean MapView::key_press_callback([[maybe_unused]] GtkEventControllerKey* controller, guint keyval, [[maybe_unused]] guint keycode, GdkModifierType state, gpointer user_data) {
    auto* self = static_cast<MapView*>(user_data);
    return self->handle_key_press(keyval, state) ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
}

// Application implementation
Application::Application() {
    setup_application();
}

Application::~Application() {
    if (app_) {
        g_object_unref(app_);
    }
}

int Application::run(int argc, char* argv[]) {
    int status = g_application_run(G_APPLICATION(app_), argc, argv);
    return status;
}

void Application::quit() {
    g_application_quit(G_APPLICATION(app_));
}

bool Application::load_map(const std::string& streets_path, const std::string& osm_path) {
    map_data_ = std::make_shared<gisevo::core::MapData>();
    return map_data_->load_from_binary(streets_path, osm_path);
}

void Application::create_main_window() {
    create_window();
}

void Application::setup_application() {
    app_ = gtk_application_new("com.gisevo.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app_, "activate", G_CALLBACK(on_activate), this);
}

void Application::create_window() {
    main_window_ = gtk_application_window_new(app_);
    gtk_window_set_title(GTK_WINDOW(main_window_), "GIS Evo - Clean Architecture");
    gtk_window_set_default_size(GTK_WINDOW(main_window_), 1024, 768);
    
    map_view_ = std::make_unique<MapView>();
    if (map_data_) {
        map_view_->set_map_data(map_data_);
    }
    
    gtk_window_set_child(GTK_WINDOW(main_window_), map_view_->widget());
    gtk_widget_grab_focus(map_view_->widget());
    
    gtk_window_present(GTK_WINDOW(main_window_));
}

// Static callback implementations
void Application::on_activate([[maybe_unused]] GtkApplication* app, gpointer user_data) {
    auto* self = static_cast<Application*>(user_data);
    self->create_window();
}

void Application::on_window_close([[maybe_unused]] GtkWidget* window, gpointer user_data) {
    auto* self = static_cast<Application*>(user_data);
    self->quit();
}

} // namespace gisevo::ui
