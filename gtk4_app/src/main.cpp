#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <iostream>

#include "core/map_data.hpp"
#include "map_selector.hpp"
#include "map_view.hpp"

namespace {

struct AppState {
  GtkApplication *app = nullptr;
  GtkWidget *window = nullptr;
  GtkStack *stack = nullptr;
  GtkWidget *map_container = nullptr;
  GtkWidget *map_title_label = nullptr;
  MapView *map_view = nullptr;
  std::shared_ptr<gisevo::core::MapData> map_data;
  std::unique_ptr<MapSelector> selector;
};

void clear_map(AppState *state) {
  if (!state) {
    return;
  }
  if (state->map_view) {
    GtkWidget *map_widget = state->map_view->widget();
    if (state->map_container && map_widget && GTK_IS_WIDGET(map_widget)) {
      GtkWidget *parent = gtk_widget_get_parent(map_widget);
      if (parent && GTK_IS_WIDGET(parent)) {
        gtk_box_remove(GTK_BOX(state->map_container), map_widget);
      }
    }
    delete state->map_view;
    state->map_view = nullptr;
  }
  state->map_data.reset();
}

void show_selector(AppState *state) {
  if (!state) {
    return;
  }
  clear_map(state);
  gtk_label_set_text(GTK_LABEL(state->map_title_label), "");
  gtk_window_set_title(GTK_WINDOW(state->window), "GIS Evo");
  gtk_stack_set_visible_child_name(state->stack, "selector");
}

void show_map(AppState *state, const MapSelector::MapEntry &entry) {
  if (!state) {
    return;
  }

  auto map_data = std::make_shared<gisevo::core::MapData>();
  if (!map_data->load_from_binary(entry.streets_path.string(), entry.osm_path.string())) {
    if (state->selector) {
      state->selector->set_status("Failed to load \"" + entry.display_name + "\".", true);
    }
    return;
  }

  clear_map(state);

  state->map_data = map_data;
  state->map_view = new MapView(map_data);
  GtkWidget *map_widget = state->map_view->widget();
  
  // Set properties BEFORE appending to prevent premature draw
  gtk_widget_set_hexpand(map_widget, TRUE);
  gtk_widget_set_vexpand(map_widget, TRUE);
  
  // Update UI state first
  gtk_label_set_text(GTK_LABEL(state->map_title_label), entry.display_name.c_str());
  gtk_window_set_title(GTK_WINDOW(state->window), ("GIS Evo - " + entry.display_name).c_str());
  
  // Append widget LAST to trigger draw only when ready
  gtk_box_append(GTK_BOX(state->map_container), map_widget);
  gtk_stack_set_visible_child_name(state->stack, "map");

  if (state->selector) {
    state->selector->set_status("", false);
  }
}

void on_back_to_maps(GtkButton *, gpointer user_data) {
  auto *state = static_cast<AppState *>(user_data);
  show_selector(state);
}

void on_window_destroy(GtkWidget *, gpointer user_data) {
  auto *state = static_cast<AppState *>(user_data);
  clear_map(state);
  delete state;
}

void on_activate(GtkApplication *app, gpointer) {
  auto *state = new AppState();
  state->app = app;

  state->window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(state->window), "GIS Evo");
  gtk_window_set_default_size(GTK_WINDOW(state->window), 1024, 768);

  state->stack = GTK_STACK(gtk_stack_new());
  gtk_stack_set_transition_type(state->stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_set_transition_duration(state->stack, 150);

  state->selector = std::make_unique<MapSelector>([state](const MapSelector::MapEntry &entry) {
    show_map(state, entry);
  });
  gtk_stack_add_named(state->stack, state->selector->widget(), "selector");

  GtkWidget *map_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(map_page, TRUE);
  gtk_widget_set_vexpand(map_page, TRUE);

  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_top(toolbar, 6);
  gtk_widget_set_margin_bottom(toolbar, 6);
  gtk_widget_set_margin_start(toolbar, 12);
  gtk_widget_set_margin_end(toolbar, 12);

  GtkWidget *back_button = gtk_button_new_with_label("Back to Maps");
  g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_to_maps), state);
  gtk_box_append(GTK_BOX(toolbar), back_button);

  state->map_title_label = gtk_label_new("");
  gtk_widget_set_hexpand(state->map_title_label, TRUE);
  gtk_label_set_xalign(GTK_LABEL(state->map_title_label), 0.0f);
  gtk_box_append(GTK_BOX(toolbar), state->map_title_label);

  gtk_box_append(GTK_BOX(map_page), toolbar);

  state->map_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(state->map_container, TRUE);
  gtk_widget_set_vexpand(state->map_container, TRUE);
  gtk_box_append(GTK_BOX(map_page), state->map_container);

  gtk_stack_add_named(state->stack, map_page, "map");

  gtk_window_set_child(GTK_WINDOW(state->window), GTK_WIDGET(state->stack));
  gtk_stack_set_visible_child_name(state->stack, "selector");

  g_signal_connect(state->window, "destroy", G_CALLBACK(on_window_destroy), state);

  gtk_window_present(GTK_WINDOW(state->window));
}

} // namespace

int main(int argc, char *argv[])
{
  GtkApplication *app = gtk_application_new("com.noahboat.gisevo", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);

  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
