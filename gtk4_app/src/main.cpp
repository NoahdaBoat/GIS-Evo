#include <gtk/gtk.h>

#include "map_view.hpp"

namespace {

void on_activate(GtkApplication *app, gpointer)
{
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "GIS Evo (GTK4 preview)");
  gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

  auto *view = new MapView();
  GtkWidget *content = view->widget();
  g_object_set_data_full(G_OBJECT(window), "map-view", view, [](gpointer data) {
    delete static_cast<MapView *>(data);
  });

  gtk_window_set_child(GTK_WINDOW(window), content);
  gtk_widget_grab_focus(content);

  gtk_window_present(GTK_WINDOW(window));
}

} // namespace

int main(int argc, char *argv[])
{
  GtkApplication *app = gtk_application_new("com.noahboat.gisevo", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);

  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
