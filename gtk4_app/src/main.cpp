#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <atomic>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <optional>
#include <array>

#include "core/map_data.hpp"
#include "map_selector.hpp"
#include "map_view.hpp"

namespace {

// Helper functions for conversion
std::optional<std::filesystem::path> find_converter_binary() {
  if (const char *env_path = std::getenv("GISEVO_OSM_CONVERTER")) {
    if (*env_path) {
      std::filesystem::path candidate(env_path);
      if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
        return candidate;
      }
    }
  }

  std::filesystem::path exe_path;
  {
    std::error_code ec;
    exe_path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
      exe_path = std::filesystem::current_path();
    }
  }
  const auto exe_dir = exe_path.parent_path();

  const std::array<std::filesystem::path, 5> candidates = {
    exe_dir / "osm_converter",
    exe_dir / "../osm_converter",
    exe_dir / "../tools/osm_converter/osm_converter",
    exe_dir / "../../tools/osm_converter/osm_converter",
    exe_dir / "../../../tools/osm_converter/osm_converter"
  };

  for (const auto &candidate : candidates) {
    if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
      return candidate;
    }
  }

  return std::nullopt;
}

std::filesystem::path find_pbf_for_bin(const std::filesystem::path& bin_path) {
  // Look for corresponding PBF file in the same directory
  std::filesystem::path bin_dir = bin_path.parent_path();
  std::string bin_name = bin_path.stem().string();
  
  // Remove .streets suffix if present
  if (bin_name.ends_with(".streets")) {
    bin_name = bin_name.substr(0, bin_name.length() - 8);
  }
  
  std::filesystem::path pbf_path = bin_dir / (bin_name + ".osm.pbf");
  if (std::filesystem::exists(pbf_path)) {
    return pbf_path;
  }
  
  return {};
}

bool convert_pbf_to_bin(const std::filesystem::path& pbf_path, const std::filesystem::path& output_dir) {
  auto converter_path = find_converter_binary();
  if (!converter_path) {
    return false;
  }

  std::string map_name = pbf_path.stem().string();
  if (map_name.ends_with(".osm")) {
    map_name = map_name.substr(0, map_name.length() - 4);
  }

  std::vector<std::string> args_storage;
  args_storage.reserve(9);
  args_storage.push_back(converter_path->string());
  args_storage.emplace_back("--input");
  args_storage.push_back(pbf_path.string());
  args_storage.emplace_back("--output-dir");
  args_storage.push_back(output_dir.string());
  args_storage.emplace_back("--map-name");
  args_storage.push_back(map_name);
  args_storage.emplace_back("--quiet");

  std::vector<const char *> argv;
  argv.reserve(args_storage.size() + 1);
  for (const auto &arg : args_storage) {
    argv.push_back(arg.c_str());
  }
  argv.push_back(nullptr);

  int result = std::system(nullptr);
  if (result == 0) {
    return false; // system not available
  }

  std::string command;
  for (size_t i = 0; i < args_storage.size(); ++i) {
    if (i > 0) command += " ";
    command += "\"" + args_storage[i] + "\"";
  }

  int exit_code = std::system(command.c_str());
  return exit_code == 0;
}

struct AppState {
  GtkApplication *app = nullptr;
  GtkWidget *window = nullptr;
  GtkStack *stack = nullptr;
  GtkWidget *map_container = nullptr;
  GtkWidget *map_title_label = nullptr;
  GtkWidget *loading_box = nullptr;
  GtkWidget *loading_label = nullptr;
  GtkWidget *loading_spinner = nullptr;
  GtkWidget *poi_toggle_button = nullptr;
  MapView *map_view = nullptr;
  std::shared_ptr<gisevo::core::MapData> map_data;
  std::unique_ptr<MapSelector> selector;
  GThread *loading_thread = nullptr;
  std::atomic<bool> loading_cancelled{false};
  
  // File paths for current map
  std::filesystem::path current_streets_path;
  std::filesystem::path current_osm_path;
  std::filesystem::path current_pbf_path;
};

void clear_map(AppState *state) {
  if (!state) {
    return;
  }
  
  // Cancel any ongoing loading
  if (state->loading_thread) {
    state->loading_cancelled.store(true);
    g_thread_join(state->loading_thread);
    state->loading_thread = nullptr;
    state->loading_cancelled.store(false);
  }
  
  // Remove loading box if present
  if (state->loading_box && state->map_container && GTK_IS_WIDGET(state->loading_box)) {
    GtkWidget *parent = gtk_widget_get_parent(state->loading_box);
    if (parent && GTK_IS_WIDGET(parent) && parent == GTK_WIDGET(state->map_container)) {
      gtk_box_remove(GTK_BOX(state->map_container), state->loading_box);
    }
    state->loading_box = nullptr;
    state->loading_label = nullptr;  // children are destroyed with parent
    state->loading_spinner = nullptr;
  }
  
  if (state->map_view) {
    GtkWidget *map_widget = state->map_view->widget();
    if (state->map_container && map_widget && GTK_IS_WIDGET(map_widget)) {
      // Check if the map widget is actually a child of the map container
      GtkWidget *parent = gtk_widget_get_parent(map_widget);
      if (parent && GTK_IS_WIDGET(parent) && parent == GTK_WIDGET(state->map_container)) {
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
  if (state->stack && GTK_IS_STACK(state->stack)) {
    gtk_stack_set_visible_child_name(state->stack, "selector");
  }
}

struct LoadMapData {
  AppState *state;
  MapSelector::MapEntry entry;
  std::shared_ptr<gisevo::core::MapData> map_data;
  bool success;
};

static gboolean map_loading_complete(gpointer user_data) {
  auto *load_data = static_cast<LoadMapData *>(user_data);
  auto *state = load_data->state;
  
  // Hide loading indicator
  if (state->loading_spinner) {
    gtk_spinner_stop(GTK_SPINNER(state->loading_spinner));
  }
  
  if (!load_data->success || state->loading_cancelled.load()) {
    // Loading failed or was cancelled
    if (state->selector && !state->loading_cancelled.load()) {
      state->selector->set_status("Failed to load \"" + load_data->entry.display_name + "\".", true);
    }
    show_selector(state);
    delete load_data;
    return G_SOURCE_REMOVE;
  }
  
  // Loading succeeded
  state->map_data = load_data->map_data;
  state->map_view = new MapView(state->map_data);
  GtkWidget *map_widget = state->map_view->widget();
  
  // Set properties BEFORE appending to prevent premature draw
  gtk_widget_set_hexpand(map_widget, TRUE);
  gtk_widget_set_vexpand(map_widget, TRUE);
  
  // Update UI state first
  gtk_label_set_text(GTK_LABEL(state->map_title_label), load_data->entry.display_name.c_str());
  gtk_window_set_title(GTK_WINDOW(state->window), ("GIS Evo - " + load_data->entry.display_name).c_str());
  
  // Remove loading widget box and append map widget
  if (state->loading_box && GTK_IS_WIDGET(state->loading_box)) {
    GtkWidget *parent = gtk_widget_get_parent(state->loading_box);
    if (parent && GTK_IS_WIDGET(parent) && parent == GTK_WIDGET(state->map_container)) {
      gtk_box_remove(GTK_BOX(state->map_container), state->loading_box);
    }
    state->loading_box = nullptr;
    state->loading_label = nullptr;  // children are destroyed with parent
    state->loading_spinner = nullptr;
  }
  
  gtk_box_append(GTK_BOX(state->map_container), map_widget);
  if (state->stack && GTK_IS_STACK(state->stack)) {
    gtk_stack_set_visible_child_name(state->stack, "map");
  }

  if (state->selector) {
    state->selector->set_status("", false);
  }
  
  state->loading_thread = nullptr;
  delete load_data;
  return G_SOURCE_REMOVE;
}

static gpointer load_map_thread_func(gpointer user_data) {
  auto *load_data = static_cast<LoadMapData *>(user_data);
  
  auto map_data = std::make_shared<gisevo::core::MapData>();
  
  // Check for cancellation before starting heavy work
  if (load_data->state->loading_cancelled.load()) {
    load_data->success = false;
    g_idle_add(map_loading_complete, load_data);
    return nullptr;
  }
  
  // Perform the actual loading (this is the slow part)
  bool success = map_data->load_from_binary(
    load_data->entry.streets_path.string(), 
    load_data->entry.osm_path.string()
  );
  
  load_data->map_data = map_data;
  load_data->success = success;
  
  // Schedule UI update on main thread
  g_idle_add(map_loading_complete, load_data);
  
  return nullptr;
}

void show_map(AppState *state, const MapSelector::MapEntry &entry) {
  if (!state) {
    return;
  }
  
  // Cancel any existing load
  if (state->loading_thread) {
    state->loading_cancelled.store(true);
    g_thread_join(state->loading_thread);
    state->loading_thread = nullptr;
    state->loading_cancelled.store(false);
  }

  clear_map(state);

  // Store file paths
  state->current_streets_path = entry.streets_path;
  state->current_osm_path = entry.osm_path;
  state->current_pbf_path = find_pbf_for_bin(entry.streets_path);

  // Check if conversion mode is enabled for this specific map and convert PBF to BIN if needed
  MapSelector::MapEntry final_entry = entry;
  if (entry.use_bin_format) {
    if (!state->current_pbf_path.empty()) {
      std::filesystem::path output_dir = state->current_pbf_path.parent_path();
      if (convert_pbf_to_bin(state->current_pbf_path, output_dir)) {
        // Update paths to point to BIN files
        std::string base_name = state->current_pbf_path.stem().string();
        if (base_name.ends_with(".osm")) {
          base_name = base_name.substr(0, base_name.length() - 4);
        }
        final_entry.streets_path = output_dir / (base_name + ".streets.bin");
        final_entry.osm_path = output_dir / (base_name + ".osm.bin");
        
        // Update stored paths
        state->current_streets_path = final_entry.streets_path;
        state->current_osm_path = final_entry.osm_path;
      }
    }
  }

  // Show loading UI
  gtk_label_set_text(GTK_LABEL(state->map_title_label), final_entry.display_name.c_str());
  gtk_window_set_title(GTK_WINDOW(state->window), ("GIS Evo - " + final_entry.display_name).c_str());
  
  // Create loading indicator
  state->loading_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_halign(state->loading_box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(state->loading_box, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand(state->loading_box, TRUE);
  gtk_widget_set_vexpand(state->loading_box, TRUE);
  
  state->loading_spinner = gtk_spinner_new();
  gtk_spinner_start(GTK_SPINNER(state->loading_spinner));
  gtk_widget_set_size_request(state->loading_spinner, 48, 48);
  gtk_box_append(GTK_BOX(state->loading_box), state->loading_spinner);
  
  state->loading_label = gtk_label_new("Loading map data...");
  gtk_widget_add_css_class(state->loading_label, "title-3");
  gtk_box_append(GTK_BOX(state->loading_box), state->loading_label);
  
  gtk_box_append(GTK_BOX(state->map_container), state->loading_box);
  if (state->stack && GTK_IS_STACK(state->stack)) {
    gtk_stack_set_visible_child_name(state->stack, "map");
  }
  
  // Start loading in background thread
  auto *load_data = new LoadMapData{state, final_entry, nullptr, false};
  state->loading_thread = g_thread_new("map-loader", load_map_thread_func, load_data);
  
  if (state->selector) {
    state->selector->set_status("Loading \"" + final_entry.display_name + "\"...", false);
  }
}

void on_back_to_maps(GtkButton *, gpointer user_data) {
  auto *state = static_cast<AppState *>(user_data);
  show_selector(state);
}

void on_poi_toggle_changed(GtkToggleButton *toggle_button, gpointer user_data) {
  auto *state = static_cast<AppState *>(user_data);
  if (state->map_view) {
    bool show_pois = gtk_toggle_button_get_active(toggle_button);
    state->map_view->set_show_pois(show_pois);
  }
}


void on_window_destroy(GtkWidget *, gpointer user_data) {
  auto *state = static_cast<AppState *>(user_data);
  
  // Cancel and wait for any loading thread
  if (state->loading_thread) {
    state->loading_cancelled.store(true);
    g_thread_join(state->loading_thread);
    state->loading_thread = nullptr;
  }
  
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

  state->poi_toggle_button = gtk_toggle_button_new_with_label("Show POIs");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(state->poi_toggle_button), TRUE);
  g_signal_connect(state->poi_toggle_button, "toggled", G_CALLBACK(on_poi_toggle_changed), state);
  gtk_box_append(GTK_BOX(toolbar), state->poi_toggle_button);

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
  if (state->stack && GTK_IS_STACK(state->stack)) {
    gtk_stack_set_visible_child_name(state->stack, "selector");
  }

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
