//
// Created by montinoa on 2/19/24.
//

#pragma once

#include <unordered_set>
#include <string>
#include "OSMEntity_Helpers/m2_way_helpers.hpp"
#include "sort_streetseg/streetsegment_info.hpp"
#include "POI/POI_helpers.hpp"
#include "ezgl/graphics.hpp"
#include "ezgl/application.hpp"
#include "ezgl/point.hpp"

struct drawing_data {
    std::string to_draw;
    ezgl::color color;
    int width;
    std::vector<ezgl::point2d> feature_points;
};

extern std::unordered_map<OSMID, int> m2_local_way_to_idx;
extern std::unordered_map<OSMID, int> m2_local_index_of_open_way;
extern ezgl::point2d highlighted_position;
extern int current_zoom_level;
extern double x_zoom_prev, y_zoom_prev;
extern std::unordered_set<IntersectionIdx> highlighted_intersections;
extern ezgl::renderer *g1;
extern std::vector<StreetSegmentIdx> highlighted_route;

/*
 * Initialize all of the call back functions
 */
void initial_setup(ezgl::application *application, bool new_window);

/*
 *
 */
void drawMap();

/*
 *
 */
void draw_main_canvas(ezgl::renderer *g);

/*
 * Draws all streets, arrows and street names
 */
void drawStreets(ezgl::renderer *g);

/*
 *
 */
void draw_features(ezgl::renderer *g);

/*
 * The function called to draw all OSMWays in the map
 */
void way_draw_features(ezgl::renderer *g);

/*
 *
 */
void combo_box_cbk(GtkComboBoxText* self, ezgl::application* app);

/*
 *
 */
void GtkTextEntry(GtkWidget*, ezgl::application* application);

/*
 * Loads all png files and organize files into vec_image_to_surface
 */
void load_image_files();

/*
 * draws the POIs based on zoom level
 */
void drawPOIPng(ezgl::renderer *g);

/*
 * draw a specific poi type
 */
void drawPOIType (ezgl::renderer *g,  std::vector<POI_info> &inner_vector, POI_category poi_category,double scale, double x1, double x2, double y1, double y2);

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
void change_map(GtkEntry* city_maps,ezgl::application* application);

/*
 * load the changed map
 */
void loadNewMap(const std::string& new_city,ezgl::application* application);

/*
 * Draws poi name based on zoom level
 */
void drawPOIName(ezgl::renderer *g,POI_class drawing_class, double text_scale,double num_scale,ezgl::point2d increment,double x_max, double x_min, double y_max,double y_min);

/*
 * call back function, only draw POI type basic
 */
void draw_ent(GtkEntry* city_maps,ezgl::application* application);

/*
 * draws all the subway routes
 */
void drawSubwayLines(ezgl::renderer *g);

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
void zoomFit(GtkEntry* zoom_fit_button, ezgl::application* application);

/*
 * Only draw subway routes and stations
 */
void draw_trans(GtkEntry* zoom_fit_button,ezgl::application* application);

/*
 * only draw POI type basic
 */
void draw_basic(GtkEntry* basic_buttom,ezgl::application* application);

/*
 * Callback function for pressing dark mode button
 */
void darkMode(GtkEntry* zoom_fit_button, ezgl::application* application);

/*
 * Clears all existing highlights on intersections
 */
void clearAllHighlights(ezgl::application* application);

/*
 * Highlight all selected intersections
 */
void drawHighlightedIntersections(ezgl::renderer* g);

/*
 * Callback function for pressing enter in search bar
 */
void searchEntryEnter(GtkEntry* search_bar, ezgl::application* application);

/*
 * Callback function for typing in search bar
 */
void searchEntryType(GtkEntry* search_bar, ezgl::application* application);

/*
 * Callback function for clicking mouse
 */
void actOnMouseClick(ezgl::application* application, GdkEventButton* event, double x, double y);

/*
 * Callback function for dialog input
 */
void dialogInput(GtkWidget *dialog ,ezgl::application *application, gpointer input);

/*
 *  function for creating dialog window and highlighting route
 */
void outputRoad(ezgl::application* application);

/*
 * Creates the popup with the 'about information'
 */
void aboutButton(GtkWidget* /*About menu button*/, ezgl::application* application);

/*
 *
 */
void helpButton(GtkWidget* /*Help button */, ezgl::application* application);


