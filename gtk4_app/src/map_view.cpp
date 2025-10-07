#include "map_view.hpp"

#include <algorithm>
#include <cmath>
#include <gdk/gdkkeysyms.h>

namespace {
constexpr double kZoomStep = 1.1;
constexpr double kMinZoom = 0.25;
constexpr double kMaxZoom = 8.0;
constexpr double kPanStep = 32.0;
}

MapView::MapView()
    : drawing_area_(gtk_drawing_area_new())
    , offset_x_(0.0)
    , offset_y_(0.0)
    , zoom_(1.0)
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
  cairo_set_source_rgb(cr, 0.07, 0.08, 0.1);
  cairo_paint(cr);

  cairo_translate(cr, width / 2.0 + offset_x_, height / 2.0 + offset_y_);
  cairo_scale(cr, zoom_, zoom_);

  cairo_set_line_width(cr, 2.0 / zoom_);
  cairo_set_source_rgb(cr, 0.2, 0.65, 0.85);
  cairo_move_to(cr, -width, 0);
  cairo_line_to(cr, width, 0);
  cairo_move_to(cr, 0, -height);
  cairo_line_to(cr, 0, height);
  cairo_stroke(cr);

  cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.4);
  cairo_set_line_width(cr, 1.0 / zoom_);
  const double grid_spacing = 100.0;
  const int grid_lines = 40;
  for(int i = -grid_lines; i <= grid_lines; ++i) {
    cairo_move_to(cr, i * grid_spacing, -grid_lines * grid_spacing);
    cairo_line_to(cr, i * grid_spacing, grid_lines * grid_spacing);
    cairo_move_to(cr, -grid_lines * grid_spacing, i * grid_spacing);
    cairo_line_to(cr, grid_lines * grid_spacing, i * grid_spacing);
  }
  cairo_stroke(cr);

  cairo_restore(cr);
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
  gtk_widget_queue_draw(drawing_area_);
}

bool MapView::handle_scroll(double, double dy)
{
  if(std::abs(dy) < 1e-6) {
    return false;
  }

  if(dy < 0) {
    zoom_ = std::min(zoom_ * kZoomStep, kMaxZoom);
  } else {
    zoom_ = std::max(zoom_ / kZoomStep, kMinZoom);
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
