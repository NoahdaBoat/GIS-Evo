//
// Created by montinoa on 2/19/24.
//

#pragma once

#include <unordered_set>
#include <string>
#include "OSMEntity_Helpers/m2_way_helpers.hpp"
#include "sort_streetseg/streetsegment_info.hpp"
#include "POI/POI_helpers.hpp"
#include <gtk/gtk.h>
#include <cairo.h>
#include "gtk4_types.hpp"

struct drawing_data {
    std::string to_draw;
    GdkRGBA color;
    int width;
    std::vector<Point2D> feature_points;
};

extern std::unordered_map<OSMID, int> m2_local_way_to_idx;
extern std::unordered_map<OSMID, int> m2_local_index_of_open_way;
extern Point2D highlighted_position;
extern int current_zoom_level;
extern double x_zoom_prev, y_zoom_prev;
extern std::unordered_set<IntersectionIdx> highlighted_intersections;
extern cairo_t *g1;
extern std::vector<StreetSegmentIdx> highlighted_route;

/*
 * Initialize all of the call back functions
 */
void initial_setup(GtkApplication *application, bool new_window);

/*
 *
 */
void drawMap();

/*
 *
 */
void draw_main_canvas(cairo_t *cr, int width, int height);

/*
 * Draws all streets, arrows and street names
 */
void drawStreets(cairo_t *cr);

/*
 *
 */
void draw_features(cairo_t *cr);

/*
 * The function called to draw all OSMWays in the map
 */
void way_draw_features(cairo_t *cr);

/*
 *
 */
void combo_box_cbk(GtkComboBoxText* self, GtkApplication* app);

/*
 *
 */
void GtkTextEntry(GtkWidget*, GtkApplication* application);

/*
 * Loads all png files and organize files into vec_image_to_surface
 */
void load_image_files();

/*
 * draws the POIs based on zoom level
 */
void drawPOIPng(cairo_t *cr);

/*
 * draw a specific poi type
 */
void drawPOIType (cairo_t *cr,  std::vector<POI_info> &inner_vector, POI_category poi_category,double scale, double x1, double x2, double y1, double y2);

/*
 * pre-load all the map names into a global variable, called in load_map()
 */
void loadMapNames();

/*
 * search the cities based on the country
 */
int searchCityCountry(std::string& country,std::unordered_map<std::string,std::string>& found_cities);

/*
 * get the map path based on the city name
 */
std::string getPathCity(std::string city, std::unordered_map<std::string, std::string> list_cities);

/*
 *
 */
bool draw_streets_check(int zoom_level, RoadType type);

/*
 * call back function, change the map
 */
void change_map(GtkEntry* city_maps,GtkApplication* application);

/*
 * load the changed map
 */
void loadNewMap(const std::string& new_city,GtkApplication* application);

/*
 * Draws poi name based on zoom level
 */
void drawPOIName(cairo_t *cr,POI_class drawing_class, double text_scale,double num_scale,Point2D increment,double x_max, double x_min, double y_max,double y_min);

/*
 * call back function, only draw POI type basic
 */
void draw_ent(GtkEntry* city_maps,GtkApplication* application);

/*
 * draws all the subway routes
 */
void drawSubwayLines(cairo_t *cr);

/*
 * Sets all of the elements in draw_which_poi to false
 */
void setAllBool(bool state);

/*
 *  Returns vector of possible intersections based on text from search bar
 */
std::vector<std::pair<IntersectionIdx, std::string>> getSearchedIntersections(GtkEntry* search_bar);

/*
 * Callback function for pressing zoom fit buttom
 */
void zoomFit(GtkEntry* zoom_fit_button, GtkApplication* application);

/*
 * Only draw subway routes and stations
 */
void draw_trans(GtkEntry* zoom_fit_button,GtkApplication* application);

/*
 * only draw POI type basic
 */
void draw_basic(GtkEntry* basic_buttom,GtkApplication* application);

/*
 * Callback function for pressing dark mode button
 */
void darkMode(GtkEntry* zoom_fit_button, GtkApplication* application);

/*
 * Clears all existing highlights on intersections
 */
void clearAllHighlights(GtkApplication* application);

/*
 * Highlight all selected intersections
 */
void drawHighlightedIntersections(cairo_t* cr);

/*
 * Callback function for pressing enter in search bar
 */
void searchEntryEnter(GtkEntry* search_bar, GtkApplication* application);

/*
 * Callback function for typing in search bar
 */
void searchEntryType(GtkEntry* search_bar, GtkApplication* application);

/*
 * Callback function for clicking mouse
 */
void actOnMouseClick(GtkApplication* application, GdkEventButton* event, double x, double y);

/*
 * Callback function for dialog input
 */
void dialogInput(GtkWidget *dialog ,GtkApplication *application, gpointer input);

/*
 *  function for creating dialog window and highlighting route
 */
void outputRoad(GtkApplication* application);

/*
 * Creates the popup with the 'about information'
 */
void aboutButton(GtkWidget* /*About menu button*/, GtkApplication* application);

/*
 *
 */
void helpButton(GtkWidget* /*Help button */, GtkApplication* application);


