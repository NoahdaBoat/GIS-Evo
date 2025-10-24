#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class MapSelector {
public:
  struct MapEntry {
    std::string display_name;
    std::filesystem::path streets_path;
    std::filesystem::path osm_path;
    bool use_bin_format = true;
  };

  using SelectionCallback = std::function<void(const MapEntry &)>;

  explicit MapSelector(SelectionCallback callback);
  MapSelector(const MapSelector &) = delete;
  MapSelector &operator=(const MapSelector &) = delete;
  MapSelector(MapSelector &&) = delete;
  MapSelector &operator=(MapSelector &&) = delete;
  ~MapSelector() = default;

  GtkWidget *widget() const;

  void refresh();
  void set_status(const std::string &message, bool is_error);
  
  // Get the list of available maps
  const std::vector<MapEntry>& get_maps() const { return maps_; }

private:
  void rebuild_list();
  void handle_open_by_index(std::size_t index);
  void update_empty_state();
  void open_converter_dialog();
  void start_conversion(const std::filesystem::path &pbf_path);
  void finish_conversion(GSubprocess *process, const std::string &stdout_text, const std::string &stderr_text, bool success);
  void set_conversion_in_progress(bool in_progress);
  void delete_map_cache(const MapEntry& entry);
  std::string get_cache_path_for_map(const MapEntry& entry) const;

  static void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
  static void on_open_clicked(GtkButton *button, gpointer user_data);
  static void on_refresh_clicked(GtkButton *button, gpointer user_data);
  static void on_convert_clicked(GtkButton *button, gpointer user_data);
  static void on_file_dialog_response(GObject *source, GAsyncResult *result, gpointer user_data);
  static void on_process_complete(GObject *source, GAsyncResult *result, gpointer user_data);
  static void on_map_conversion_switch_changed(GtkSwitch *switch_widget, gboolean state, gpointer user_data);
  static void on_cache_menu_clicked(GtkButton *button, gpointer user_data);
  static void on_delete_cache_clicked(GtkButton *button, gpointer user_data);

  GtkWidget *root_;
  GtkListBox *list_box_;
  GtkWidget *status_label_;
  GtkWidget *empty_label_;
  GtkWidget *convert_button_;
  SelectionCallback callback_;
  std::vector<MapEntry> maps_;
  GSubprocess *current_process_ = nullptr;
  bool conversion_in_progress_ = false;
};
