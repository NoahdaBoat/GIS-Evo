#include "map_selector.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <gio/gio.h>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace {

constexpr const char kStreetsSuffix[] = ".streets.bin";

bool ends_with(const std::string &value, const std::string &suffix) {
  if (value.size() < suffix.size()) {
    return false;
  }
  return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string prettify_name(std::string base) {
  for (char &ch : base) {
    if (ch == '_' || ch == '-') {
      ch = ' ';
    }
  }
  if (!base.empty()) {
    base[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(base[0])));
  }
  return base;
}

std::filesystem::path normalize_path(const std::filesystem::path &path) {
  std::error_code ec;
  auto absolute_path = std::filesystem::absolute(path, ec);
  if (!ec) {
    return absolute_path.lexically_normal();
  }
  return path.lexically_normal();
}

bool is_executable(const std::filesystem::path &path) {
  if (path.empty()) {
    return false;
  }
  auto path_str = path.string();
  return g_file_test(path_str.c_str(), G_FILE_TEST_IS_EXECUTABLE);
}

std::vector<std::filesystem::path> candidate_directories() {
  std::vector<std::filesystem::path> directories;
  std::unordered_set<std::string> seen;

  auto add_directory = [&](const std::filesystem::path &path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
      return;
    }
    auto normalized = normalize_path(path);
    auto key = normalized.string();
    if (seen.insert(key).second) {
      directories.push_back(normalized);
    }
  };

  if (const char *env_dir = std::getenv("GISEVO_MAPS_DIR")) {
    if (*env_dir != '\0') {
      add_directory(std::filesystem::path(env_dir));
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

  add_directory(exe_dir / "resources/maps");
  add_directory(exe_dir / "../resources/maps");
  add_directory(exe_dir / "../../resources/maps");

  const auto cwd = std::filesystem::current_path();
  add_directory(cwd / "resources/maps");
  add_directory(cwd / "../resources/maps");
  add_directory(cwd / "../../resources/maps");

  return directories;
}

std::optional<std::filesystem::path> find_converter_binary() {
  if (const char *env_path = std::getenv("GISEVO_OSM_CONVERTER")) {
    if (*env_path) {
      std::filesystem::path candidate(env_path);
      if (is_executable(candidate)) {
        return normalize_path(candidate);
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
    if (is_executable(candidate)) {
      return normalize_path(candidate);
    }
  }

  if (char *found = g_find_program_in_path("osm_converter")) {
    std::filesystem::path candidate(found);
    g_free(found);
    if (is_executable(candidate)) {
      return normalize_path(candidate);
    }
  }

  return std::nullopt;
}

std::string sanitize_map_name(std::string name) {
  if (name.empty()) {
    return "converted_map";
  }
  for (char &ch : name) {
    if (!std::isalnum(static_cast<unsigned char>(ch))) {
      ch = '_';
    }
  }
  // Collapse repeated underscores
  std::string result;
  result.reserve(name.size());
  bool last_was_underscore = false;
  for (char ch : name) {
    if (ch == '_') {
      if (!last_was_underscore) {
        result.push_back(ch);
      }
      last_was_underscore = true;
    } else {
      result.push_back(ch);
      last_was_underscore = false;
    }
  }
  if (!result.empty() && result.front() == '_') {
    result.erase(result.begin());
  }
  if (result.empty()) {
    return "converted_map";
  }
  return result;
}

std::vector<MapSelector::MapEntry> discover_maps() {
  std::vector<MapSelector::MapEntry> maps;
  std::unordered_set<std::string> seen;

  for (const auto &directory : candidate_directories()) {
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(directory, ec)) {
      if (ec) {
        break;
      }
      std::error_code status_ec;
      if (!entry.is_regular_file(status_ec)) {
        continue;
      }

      const auto path = entry.path();
      const auto filename = path.filename().string();
      if (!ends_with(filename, kStreetsSuffix)) {
        continue;
      }

      const auto base = filename.substr(0, filename.size() - std::char_traits<char>::length(kStreetsSuffix));
      const auto osm_path = directory / (base + ".osm.bin");
      std::error_code exists_ec;
      if (!std::filesystem::exists(osm_path, exists_ec)) {
        continue;
      }

      const auto canonical_key = normalize_path(path).string();
      if (!seen.insert(canonical_key).second) {
        continue;
      }

      MapSelector::MapEntry map_entry;
      map_entry.display_name = prettify_name(base);
      map_entry.streets_path = path;
      map_entry.osm_path = osm_path;
      maps.push_back(std::move(map_entry));
    }
  }

  std::sort(maps.begin(), maps.end(), [](const auto &lhs, const auto &rhs) {
    if (lhs.display_name == rhs.display_name) {
      return lhs.streets_path < rhs.streets_path;
    }
    return lhs.display_name < rhs.display_name;
  });

  return maps;
}

} // namespace

MapSelector::MapSelector(SelectionCallback callback)
    : root_(gtk_box_new(GTK_ORIENTATION_VERTICAL, 12)),
      list_box_(GTK_LIST_BOX(gtk_list_box_new())),
      status_label_(gtk_label_new("")),
      empty_label_(gtk_label_new("No maps found. Place .streets.bin and .osm.bin files in the maps directory.")),
      callback_(std::move(callback)) {
  gtk_widget_set_margin_top(root_, 16);
  gtk_widget_set_margin_bottom(root_, 16);
  gtk_widget_set_margin_start(root_, 24);
  gtk_widget_set_margin_end(root_, 24);

  auto *title = gtk_label_new("Select a map to launch");
  gtk_widget_add_css_class(title, "title-3");
  gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
  gtk_box_append(GTK_BOX(root_), title);

  auto *description = gtk_label_new("Maps are discovered from GISEVO_MAPS_DIR or nearby resources/maps folders.");
  gtk_label_set_xalign(GTK_LABEL(description), 0.0f);
  gtk_label_set_wrap(GTK_LABEL(description), TRUE);
  gtk_box_append(GTK_BOX(root_), description);

  gtk_list_box_set_selection_mode(list_box_, GTK_SELECTION_NONE);
  g_signal_connect(list_box_, "row-activated", G_CALLBACK(MapSelector::on_row_activated), this);

  auto *scrolled = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_widget_set_hexpand(scrolled, TRUE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), GTK_WIDGET(list_box_));
  gtk_box_append(GTK_BOX(root_), scrolled);

  gtk_label_set_xalign(GTK_LABEL(empty_label_), 0.5f);
  gtk_label_set_wrap(GTK_LABEL(empty_label_), TRUE);
  gtk_widget_set_visible(empty_label_, FALSE);
  gtk_box_append(GTK_BOX(root_), empty_label_);

  auto *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  convert_button_ = gtk_button_new_with_label("Add Map from PBF");
  g_signal_connect(convert_button_, "clicked", G_CALLBACK(MapSelector::on_convert_clicked), this);
  gtk_box_append(GTK_BOX(controls), convert_button_);

  auto *refresh_button = gtk_button_new_with_label("Refresh");
  g_signal_connect(refresh_button, "clicked", G_CALLBACK(MapSelector::on_refresh_clicked), this);
  gtk_box_append(GTK_BOX(controls), refresh_button);
  gtk_box_append(GTK_BOX(root_), controls);

  gtk_label_set_xalign(GTK_LABEL(status_label_), 0.0f);
  gtk_label_set_wrap(GTK_LABEL(status_label_), TRUE);
  gtk_widget_set_margin_top(status_label_, 6);
  gtk_box_append(GTK_BOX(root_), status_label_);

  refresh();
  set_conversion_in_progress(false);
}

GtkWidget *MapSelector::widget() const {
  return root_;
}

void MapSelector::refresh() {
  maps_ = discover_maps();
  rebuild_list();
  update_empty_state();

  if (maps_.empty()) {
    set_status("No maps available. Place map binaries in the configured maps directory and refresh.", true);
  } else {
    set_status(std::to_string(maps_.size()) + " map(s) available.", false);
  }

  set_conversion_in_progress(conversion_in_progress_);
}

void MapSelector::set_status(const std::string &message, bool is_error) {
  if (message.empty()) {
    gtk_label_set_text(GTK_LABEL(status_label_), "");
    gtk_widget_remove_css_class(status_label_, "error");
    return;
  }

  if (is_error) {
    gtk_label_set_text(GTK_LABEL(status_label_), ("Error: " + message).c_str());
    gtk_widget_add_css_class(status_label_, "error");
  } else {
    gtk_label_set_text(GTK_LABEL(status_label_), message.c_str());
    gtk_widget_remove_css_class(status_label_, "error");
  }
}

void MapSelector::rebuild_list() {
  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(list_box_));
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_list_box_remove(list_box_, child);
    child = next;
  }

  for (std::size_t index = 0; index < maps_.size(); ++index) {
    auto *row = gtk_list_box_row_new();

    auto *outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(outer, 8);
    gtk_widget_set_margin_bottom(outer, 8);
    gtk_widget_set_margin_start(outer, 12);
    gtk_widget_set_margin_end(outer, 12);

    auto *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(info_box, TRUE);

    auto *name_label = gtk_label_new(maps_[index].display_name.c_str());
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
    gtk_widget_add_css_class(name_label, "title-4");

    auto path_string = maps_[index].streets_path.string();
    auto *path_label = gtk_label_new(path_string.c_str());
    gtk_label_set_xalign(GTK_LABEL(path_label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(path_label), TRUE);
    gtk_widget_add_css_class(path_label, "dim-label");

    gtk_box_append(GTK_BOX(info_box), name_label);
    gtk_box_append(GTK_BOX(info_box), path_label);

    auto *open_button = gtk_button_new_with_label("Open");
    gtk_widget_set_valign(open_button, GTK_ALIGN_CENTER);
    g_object_set_data(G_OBJECT(open_button), "map-index", GINT_TO_POINTER(static_cast<int>(index)));
    g_signal_connect(open_button, "clicked", G_CALLBACK(MapSelector::on_open_clicked), this);

    gtk_box_append(GTK_BOX(outer), info_box);
    gtk_box_append(GTK_BOX(outer), open_button);

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), outer);
    g_object_set_data(G_OBJECT(row), "map-index", GINT_TO_POINTER(static_cast<int>(index)));

    gtk_list_box_append(list_box_, row);
  }
}

void MapSelector::handle_open_by_index(std::size_t index) {
  if (index >= maps_.size()) {
    set_status("Selected map is no longer available.", true);
    return;
  }

  if (callback_) {
    set_status("Loading \"" + maps_[index].display_name + "\"...", false);
    callback_(maps_[index]);
  }
}

void MapSelector::update_empty_state() {
  gtk_widget_set_visible(empty_label_, maps_.empty());
  gtk_widget_set_sensitive(GTK_WIDGET(list_box_), !maps_.empty());
}

void MapSelector::open_converter_dialog() {
  if (conversion_in_progress_) {
    set_status("A conversion is already running.", true);
    return;
  }

  GtkFileDialog *dialog = gtk_file_dialog_new();

  GtkFileFilter *pbf_filter = gtk_file_filter_new();
  gtk_file_filter_set_name(pbf_filter, "OpenStreetMap PBF (*.osm.pbf)");
  gtk_file_filter_add_pattern(pbf_filter, "*.osm.pbf");

  GtkFileFilter *all_filter = gtk_file_filter_new();
  gtk_file_filter_set_name(all_filter, "All files");
  gtk_file_filter_add_pattern(all_filter, "*");
  
  GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
  g_list_store_append(filters, pbf_filter);
  g_list_store_append(filters, all_filter);
  gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

  g_object_unref(pbf_filter);
  g_object_unref(all_filter);

  GtkWindow *parent = nullptr;
  if (GtkRoot *root = gtk_widget_get_root(root_)) {
    if (GTK_IS_WINDOW(root)) {
      parent = GTK_WINDOW(root);
    }
  }

  gtk_file_dialog_open(dialog, parent, nullptr, MapSelector::on_file_dialog_response, this);
  g_object_unref(dialog);
}

void MapSelector::set_conversion_in_progress(bool in_progress) {
  conversion_in_progress_ = in_progress;
  if (convert_button_) {
    gtk_widget_set_sensitive(convert_button_, !in_progress);
  }
  gtk_widget_set_sensitive(GTK_WIDGET(list_box_), !in_progress && !maps_.empty());
}

void MapSelector::start_conversion(const std::filesystem::path &pbf_path) {
  if (conversion_in_progress_) {
    set_status("A conversion is already running.", true);
    return;
  }

  if (!std::filesystem::exists(pbf_path)) {
    set_status("Input file not found: " + pbf_path.string(), true);
    return;
  }

  auto converter_path = find_converter_binary();
  if (!converter_path) {
    set_status("Unable to locate osm_converter executable. Set GISEVO_OSM_CONVERTER or install the tool.", true);
    return;
  }

  auto directories = candidate_directories();
  std::filesystem::path output_dir;
  if (!directories.empty()) {
    output_dir = directories.front();
  } else {
    output_dir = std::filesystem::current_path() / "resources/maps";
  }

  std::error_code dir_ec;
  std::filesystem::create_directories(output_dir, dir_ec);
  if (dir_ec) {
    set_status("Failed to prepare output directory: " + output_dir.string(), true);
    return;
  }

  std::filesystem::path stem = pbf_path.stem();
  std::string map_name = stem.string();
  if (stem.has_extension()) {
    map_name = stem.stem().string();
  }
  map_name = sanitize_map_name(map_name);

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

  GError *error = nullptr;
  auto flags = static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
  GSubprocess *process = g_subprocess_newv(argv.data(), flags, &error);
  if (!process) {
    std::string message = error && error->message ? error->message : "Failed to start converter.";
    set_status(message, true);
    g_clear_error(&error);
    return;
  }

  if (current_process_) {
    g_clear_object(&current_process_);
  }
  current_process_ = process;
  set_conversion_in_progress(true);

  set_status("Converting \"" + pbf_path.filename().string() + "\"...", false);
  g_subprocess_communicate_utf8_async(process, nullptr, nullptr, MapSelector::on_process_complete, this);
}

void MapSelector::finish_conversion(GSubprocess *process, const std::string &stdout_text, const std::string &stderr_text, bool success) {
  (void)process;
  if (current_process_) {
    g_clear_object(&current_process_);
  }

  set_conversion_in_progress(false);

  if (success) {
    refresh();
    std::string message = "Conversion completed.";
    if (!stdout_text.empty()) {
      message += " " + stdout_text;
    }
    set_status(message, false);
  } else {
    auto extract_summary = [](const std::string &text) {
      std::string summary;
      for (char ch : text) {
        if (ch == '\n' || ch == '\r') {
          break;
        }
        summary.push_back(ch);
      }
      return summary;
    };

    std::string message = "Conversion failed.";
    if (!stderr_text.empty()) {
      std::string summary = extract_summary(stderr_text);
      if (!summary.empty()) {
        message += " " + summary;
      }
    }
    set_status(message, true);
  }

}

void MapSelector::on_row_activated(GtkListBox *, GtkListBoxRow *row, gpointer user_data) {
  auto *self = static_cast<MapSelector *>(user_data);
  if (!self) {
    return;
  }
  int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "map-index"));
  if (index >= 0) {
    self->handle_open_by_index(static_cast<std::size_t>(index));
  }
}

void MapSelector::on_open_clicked(GtkButton *button, gpointer user_data) {
  auto *self = static_cast<MapSelector *>(user_data);
  if (!self) {
    return;
  }
  int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "map-index"));
  if (index >= 0) {
    self->handle_open_by_index(static_cast<std::size_t>(index));
  }
}

void MapSelector::on_refresh_clicked(GtkButton *, gpointer user_data) {
  auto *self = static_cast<MapSelector *>(user_data);
  if (!self) {
    return;
  }
  self->refresh();
}

void MapSelector::on_convert_clicked(GtkButton *, gpointer user_data) {
  auto *self = static_cast<MapSelector *>(user_data);
  if (!self) {
    return;
  }
  self->open_converter_dialog();
}

void MapSelector::on_file_dialog_response(GObject *source, GAsyncResult *result, gpointer user_data) {
  auto *self = static_cast<MapSelector *>(user_data);
  if (!self) {
    return;
  }

  auto *dialog = GTK_FILE_DIALOG(source);
  GError *error = nullptr;
  GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);

  if (error) {
    self->set_status(error->message ? error->message : "Failed to select file.", true);
    g_error_free(error);
    return;
  }

  if (!file) {
    self->set_status("Conversion cancelled.", false);
    return;
  }

  char *path_cstr = g_file_get_path(file);
  g_object_unref(file);

  if (!path_cstr) {
    self->set_status("Selected file is not on a local filesystem.", true);
    return;
  }

  std::filesystem::path selected_path(path_cstr);
  g_free(path_cstr);

  self->start_conversion(selected_path);
}

void MapSelector::on_process_complete(GObject *source, GAsyncResult *result, gpointer user_data) {
  auto *self = static_cast<MapSelector *>(user_data);
  if (!self) {
    return;
  }

  auto *process = G_SUBPROCESS(source);
  gchar *stdout_buf = nullptr;
  gchar *stderr_buf = nullptr;
  GError *error = nullptr;
  gboolean ok = g_subprocess_communicate_utf8_finish(process, result, &stdout_buf, &stderr_buf, &error);

  std::string stdout_text = stdout_buf ? stdout_buf : "";
  std::string stderr_text = stderr_buf ? stderr_buf : "";
  g_free(stdout_buf);
  g_free(stderr_buf);

  bool success = false;

  if (!ok) {
    std::string message = error && error->message ? error->message : "Conversion failed.";
    if (!stderr_text.empty()) {
      message += " " + stderr_text;
    }
    self->set_status(message, true);
    g_clear_error(&error);
  } else {
    success = g_subprocess_get_if_exited(process) && g_subprocess_get_exit_status(process) == 0;
    self->finish_conversion(process, stdout_text, stderr_text, success);
  }

  if (!success && !ok) {
    self->set_conversion_in_progress(false);
    if (self->current_process_) {
      g_clear_object(&self->current_process_);
    }
  }
}
