#pragma once

#include <gtk/gtk.h>

class MapView {
public:
  MapView();
  MapView(const MapView &) = delete;
  MapView &operator=(const MapView &) = delete;
  MapView(MapView &&) = delete;
  MapView &operator=(MapView &&) = delete;
  ~MapView() = default;

  GtkWidget *widget() const;

private:
  GtkWidget *drawing_area_;
  double offset_x_;
  double offset_y_;
  double zoom_;
  double drag_start_x_;
  double drag_start_y_;

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
