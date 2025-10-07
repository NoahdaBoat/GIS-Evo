/* 
 * Copyright 2024 University of Toronto
 *
 * Permission is hereby granted, to use this software and associated 
 * documentation files (the "Software") in course work at the University 
 * of Toronto, or for personal use. Other uses are prohibited, in 
 * particular the distribution of the Software either publicly or to third 
 * parties.
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ezgl/application.hpp"
#include "ezgl/graphics.hpp"
#include "ezgl/canvas.hpp"
#include "ms1helpers.h"
#include "ms2helpers.hpp"
#include "ms3helpers.hpp"
#include "globals.h"
#include "Coordinates_Converstions/coords_conversions.hpp"
#include "OSMEntity_Helpers/m2_way_helpers.hpp"
#include "OSMEntity_Helpers/typed_osmid_helper.hpp"
#include "Intersections/intersection_setup.hpp"
#include "sort_streetseg/streetsegment_info.hpp"
#include "astaralgo.hpp"
#include "ms4helpers.hpp"
#include "m4.h"

// std library
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <deque>
#include <chrono>
#include <thread>

#define VISUALIZE

// Set up the ezgl graphics window and hand control to it, as shown in the
// ezgl example program.
// This function will be called by both the unit tests (ece297exercise)
// and your main() function in main/src/main.cpp.
// The unit tests always call loadMap() before calling this function
// and call closeMap() after this function returns.


// local globals
std::vector<way_info> m2_local_all_ways_info;
std::vector<feature_data> m2_local_all_features_info;
std::unordered_map<OSMID, feature_data*> m2_local_id_to_feature;
std::unordered_map<OSMID, int> m2_local_way_to_idx;
std::unordered_map<OSMID, int> m2_local_index_of_open_way;
std::vector<RoadType> m2_local_all_street_types;
std::vector<each_relation> m2_local_all_relations_vector;
std::vector<feature_info> closed_features;
std::vector<feature_info> open_features;
std::pair<IntersectionIdx, ezgl::point2d> clicked_intersection;
std::pair<IntersectionIdx, ezgl::point2d> origin_intersection;
std::pair<IntersectionIdx, ezgl::point2d> destination_intersection;
std::unordered_set<IntersectionIdx> highlighted_intersections;
std::vector<StreetSegmentIdx> highlighted_route;
int draw_index = 0;
bool draw_path = false;
int current_zoom_level = 0;
double x_zoom_prev, y_zoom_prev;
bool valid_input = false;
StreetSegmentIdx street_to_highlight = -1;
ezgl::application* global_access;
bool search_route = false;
bool set_origin = true;


void clearAllHighlights(ezgl::application* application) {

    for (int i = 0; i < getNumIntersections(); ++i) {
        intersection_info& info = globals.all_intersections[i];
        info.highlight = false;
    }
    application->refresh_drawing();
    highlighted_intersections.clear();
}


std::vector<std::pair<IntersectionIdx, std::string>> getSearchedIntersections(GtkEntry* search_bar) {

    valid_input = false;

    // converts gchar from search bar into stringstream
    const gchar* location_input_gchar = gtk_entry_get_text(search_bar);
    std::string location_input_str(location_input_gchar);
    std::stringstream location_input_ss(location_input_str);
    std::string street_string_1, street_string_2, word;

    // load stringstream into street_string_1 and street_string_2
    while (location_input_ss >> word) {

        if (word == "&"){
            break;
        }
        street_string_1 += word;
    }

    while (location_input_ss >> word) {
        street_string_2 += word;
        valid_input = true;
    }

    // find all intersections that the two streets intersect at
    std::vector<StreetIdx> streets_vec_1 = findStreetIdsFromPartialStreetName(street_string_1);
    std::vector<StreetIdx> streets_vec_2 = findStreetIdsFromPartialStreetName(street_string_2);

    // searched_intersections contains a vector of pair<IntersectionIdx, intersection name>
    // of all intersection suggestions based on user input
    std::vector<std::pair<IntersectionIdx, std::string>> searched_intersections;
    std::unordered_set<std::string> processed_intersections;
    std::string message;

    // if second street name is not typed in
    if (street_string_2.size() == 0){
        for (int i = 0; i < streets_vec_1.size(); i++){
            std::vector<IntersectionIdx> more_intersections = globals.vec_streetinfo[streets_vec_1[i]].intersections;

            for (int j = 0; j < more_intersections.size(); j++){
                std::string sug_intersection_name = getIntersectionName(more_intersections[j]);
                size_t amp_position = sug_intersection_name.find('&');
                std::string sug_street_name_1 = sug_intersection_name.substr(0, amp_position - 1);
                std::string sug_street_name_2 = sug_intersection_name.substr(amp_position + 2);

                // do not show intersection name if either street names are unknown or
                // if second street name contains & or intersection name does not contain &
                if (sug_street_name_1 == "<unknown>" || sug_street_name_2 == "<unknown>" ||
                    sug_intersection_name.find('&') == std::string::npos || sug_street_name_2.find('&') != std::string::npos){
                    continue;
                }

                // if street name entered does not equal to first street name of suggested intersection
                if (sug_street_name_1 != getStreetName(streets_vec_1[i])){
                    sug_intersection_name = sug_street_name_2 + " & " + sug_street_name_1;
                }

                // checking for duplicates
                if (processed_intersections.find(sug_intersection_name) == processed_intersections.end()){
                    searched_intersections.push_back({more_intersections[j], sug_intersection_name});
                    processed_intersections.insert(sug_intersection_name);
                }
            }
        }
    }

    // if second street name is typed in
    else{
        for (int i = 0; i < streets_vec_1.size(); i++){
            for (int j = 0; j < streets_vec_2.size(); j++){
                StreetIdx street_1 = streets_vec_1[i];
                StreetIdx street_2 = streets_vec_2[j];
                std::string street_name_1 = getStreetName(street_1);
                std::string street_name_2 = getStreetName(street_2);

                // skip intersections with more than 2 street names
                if (street_name_1.find('&') != std::string::npos || street_name_2.find('&') != std::string::npos){
                    continue;
                }

                std::pair<StreetIdx, StreetIdx> street_ids(street_1, street_2);
                std::vector<IntersectionIdx> more_intersections = findIntersectionsOfTwoStreets(street_ids);

                for (int k = 0; k < more_intersections.size(); k++){

                    std::string int_name = street_name_1 + " & " + street_name_2;

                    if (street_name_1 != street_name_2 && street_name_1 != "<unknown>" && street_name_2 != "<unknown>" &&
                        processed_intersections.find(int_name) == processed_intersections.end()){
                        searched_intersections.push_back({more_intersections[k], int_name});
                        processed_intersections.insert(int_name);
                    }
                }
            }
        }
    }

    // searched_intersections contains all valid intersections names with no duplicates
    return searched_intersections;
}


void searchEntryEnter(GtkEntry* search_bar, ezgl::application* application) {

    if (!valid_input){
        std::string message = "Invalid Intersection";
        application->create_popup_message("Warning", message.c_str());
        return;
    }

    // save previous state of origin_intersection and destination_intersection
    bool origin_highlighted = globals.all_intersections[origin_intersection.first].highlight;
    bool destination_highlighted = globals.all_intersections[destination_intersection.first].highlight;

    clearAllHighlights(application);

    // user pressed enter in search_route mode
    if (search_route){

        // in origin text entry
        if (G_OBJECT(search_bar) == application->get_object("OriginSearch")){
            if (destination_highlighted){
                globals.all_intersections[destination_intersection.first].highlight = true;
            }
            globals.all_intersections[origin_intersection.first].highlight = true;
        }

        // in destination text entry
        else {
            if (origin_highlighted){
                globals.all_intersections[origin_intersection.first].highlight = true;
            }
            globals.all_intersections[destination_intersection.first].highlight = true;
            outputRoad(application);
        }
        application->refresh_drawing();
    }

    // user pressed enter and not in search_route mode
    else{
        std::vector<std::pair<IntersectionIdx, std::string>> searched_intersections = getSearchedIntersections(search_bar);
        std::string message;

        // display at max 5 intersection information at once
        for (int i = 0; i < std::min(static_cast<size_t>(5), searched_intersections.size()); i++){
            intersection_info& info = globals.all_intersections[searched_intersections[i].first];

            highlighted_intersections.insert(searched_intersections[i].first);
            info.highlight = true;
            message += "Intersection Name: " + searched_intersections[i].second + "\n";
            message += "Longitude: " + std::to_string(x_to_lon(info.position.x)) + "\n";
            message += "Latitude: " + std::to_string(y_to_lat(info.position.y)) + "\n";
        }

        if (searched_intersections.size() == 0){
            message += "                 No intersection                 ";
        }

        application->refresh_drawing();

        application->create_popup_message("Intersection(s) Information", message.c_str());
    }
}

void searchEntryType(GtkEntry* search_bar, ezgl::application* application) {

    // load data into list_store
    GtkListStore* list_store = GTK_LIST_STORE(application->get_object("ListStore"));
    gtk_list_store_clear(list_store);

    std::vector<std::pair<IntersectionIdx, std::string>> searched_intersections = getSearchedIntersections(search_bar);
    std::vector<std::string> searched_intersections_name;

    // sort the searched_intersection alphabetically by street name
    for (int i = 0; i < searched_intersections.size(); i++){
        searched_intersections_name.push_back(searched_intersections[i].second);
    }
    std::sort(searched_intersections_name.begin(), searched_intersections_name.end());
    searched_intersections_name.resize(15);

    // insert searched_intersections_name into list_store
    for (int i = 0; i < searched_intersections_name.size(); i++){
        GtkTreeIter iterator;
        gtk_list_store_append(list_store, &iterator);
        gtk_list_store_set(list_store, &iterator, 0, searched_intersections_name[i].c_str(), -1);
    }

    // update the origin and destination positions based on the first suggested intersection searched
    for (int i = 0; i < searched_intersections.size(); i++){
        if (searched_intersections[i].second == searched_intersections_name[0]){
            if (G_OBJECT(search_bar) == application->get_object("OriginSearch")){
                origin_intersection.first = searched_intersections[i].first;
                origin_intersection.second = globals.all_intersections[searched_intersections[i].first].position;
            }
            else{
                destination_intersection.first = searched_intersections[i].first;
                destination_intersection.second = globals.all_intersections[searched_intersections[i].first].position;
            }
        }
    }
}

void zoomFit(GtkEntry* /*zoom_fit_button*/, ezgl::application* application) {
    current_zoom_level = 1;
    application->refresh_drawing();
}

void change_map(GtkEntry* city_maps, ezgl::application* application) {
    GtkComboBoxText* list_cities = GTK_COMBO_BOX_TEXT(city_maps);
    gchar* selected_city = gtk_combo_box_text_get_active_text(list_cities);
    std::string new_city = selected_city;
    std::cout<<"Changing to: " << new_city << std::endl;
    loadNewMap(new_city, application);
}

void draw_ent(GtkEntry* /*ent_buttom*/, ezgl::application* application) {
    if(globals.draw_which_poi[NUM_POI_class+1]) {
        setAllBool(false);
    }
        globals.draw_which_poi[entertainment] = !globals.draw_which_poi[entertainment] ;
    application->refresh_drawing();
}

void draw_trans(GtkEntry* /*trans_buttom*/, ezgl::application* application) {
    if(globals.draw_which_poi[NUM_POI_class+1]) {
        setAllBool(false);
    }
    globals.draw_which_poi[station] = !globals.draw_which_poi[station] ;
    application->refresh_drawing();
}

void draw_basic(GtkEntry* /*basic_buttom*/, ezgl::application* application) {
    if(globals.draw_which_poi[NUM_POI_class+1]) {
        setAllBool(false);
    }
    globals.draw_which_poi[basic] = !globals.draw_which_poi[basic] ;
    application->refresh_drawing();
}

void darkMode(GtkEntry* /*dark_mode_button*/, ezgl::application* application) {
   globals.dark_mode = !globals.dark_mode;
   application->refresh_drawing();
    // draw_path = true;
    // draw_index+=10;
    application->refresh_drawing();
}

void aboutButton(GtkWidget* /*About menu button*/, ezgl::application* application) {
    std::string message;

    message += std::string("Created by: ") + "\n";
    message += std::string("Lanlin He, Nicole Jiao, and Noah Monti") + "\n";
    message += std::string("At the University of Toronto") + "\n";
    message += std::string("-- 2024 --");

    application->create_popup_message("About", message.c_str());
}

void helpButton(GtkWidget* /*Help button */, ezgl::application* application) {
    std::string message;

    message += std::string("Information about this application: ") + "\n" + "\n";
    message += std::string("To search for an intersection, or find a route: Enter the starting intersection in the 'Origin' box, and enter the destination intersection in the 'Destination' box") + "\n" + "\n";
    message += std::string("Points of interest are represented across the map, clicking on one will display its information") + "\n" + "\n";

    application->create_popup_message("Help", message.c_str());

}

void outputRoad(ezgl::application* application) {
    highlighted_route.clear();
    highlighted_route = findPathBetweenIntersections(15, std::make_pair(origin_intersection.first, destination_intersection.first));

    // highlight start and destination:
    globals.all_intersections[destination_intersection.first].highlight = true;
    globals.all_intersections[origin_intersection.first].highlight = true;

    // create dynamic dialog window
    GtkWindow* window = GTK_WINDOW(application->get_object(application->get_main_window_id().c_str()));
    GtkWidget* dialog = gtk_dialog_new_with_buttons("Route Window", window, GTK_DIALOG_DESTROY_WITH_PARENT, "DONE", GTK_RESPONSE_ACCEPT, NULL);
    // make it transient for main window
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_window_set_default_size(GTK_WINDOW(dialog),200,300);

    //create a scrollable window
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled_window,TRUE);
    gtk_widget_set_vexpand(scrolled_window,TRUE);

    //create a box to hold multiple texts
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
    gtk_container_add(GTK_CONTAINER(scrolled_window), box);

    // Create texts to be added
    // display destinations and start point
    std::string display = "Fastest Route From " + globals.all_intersections[origin_intersection.first].name + " to: " + globals.all_intersections[destination_intersection.first].name;
    const gchar* disp_char = display.c_str();
    GtkWidget* label = gtk_label_new(disp_char);
    gtk_box_pack_start(GTK_BOX(box),label,TRUE, TRUE, 0);

    // display total travel time
    std::string travel = "Estimated Travel Time: " + std::to_string(computePathTravelTime(15, highlighted_route))+ " s.";
    const gchar* travel_char = travel.c_str();
    GtkWidget* travel_label = gtk_label_new(travel_char);
    gtk_box_pack_start(GTK_BOX(box),travel_label,TRUE, TRUE, 0);;

    std::string start = globals.all_street_segments[highlighted_route[0]].inter_from;
    start = "Starting at: "+start +"on "+globals.all_street_segments[highlighted_route[0]].street_name;
    const gchar *start_name = start.c_str();
    GtkWidget *start_segment = gtk_label_new(start_name);
    gtk_box_pack_start(GTK_BOX(box),start_segment,TRUE, TRUE, 0);

    // display the directions via text
    std::vector<std::string> directions = findDirections(highlighted_route);
    StreetIdx current_strt = globals.all_street_segments[highlighted_route[0]].street;
    for (int i = 1; i <highlighted_route.size(); i++) {
        StreetSegmentIdx segment = highlighted_route[i];
        std::string street = globals.all_street_segments[segment].street_name;
        StreetIdx streetIdx =  globals.all_street_segments[segment].street;
        if (streetIdx != current_strt) {
            current_strt =streetIdx;
            street = directions[i-1] + globals.all_street_segments[segment].inter_to + " || towards: " + street;
            const gchar *street_name = street.c_str();
            GtkWidget *strt_segment = gtk_label_new(street_name);
            gtk_box_pack_start(GTK_BOX(box),strt_segment,FALSE, FALSE, 0);
        }
    }

    //add scrolled window to the dialog
    auto content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content_area),scrolled_window);

    // show all the widgets in the dialog
    gtk_widget_show_all(dialog);

    // move the location of the dialog to the side of the screen
    // get location of window
    gint window_pos_x, window_pos_y, dialog_pos_x, dialog_pos_y;
    gtk_window_get_position(GTK_WINDOW(window), &window_pos_x, &window_pos_y);
    gint width = gtk_widget_get_allocated_width(reinterpret_cast<GtkWidget *>(window));
    ezgl::rectangle work_area;
    gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()), reinterpret_cast<GdkRectangle *>(&work_area));
    //new position: take the min value of either the main window side or the screen side
    dialog_pos_x=std::min((int)window_pos_x+width,(int)(work_area.width()-gtk_widget_get_allocated_width(dialog)));
    dialog_pos_y = window_pos_y;
    gtk_window_move(GTK_WINDOW(dialog), dialog_pos_x, dialog_pos_y);

    GObject* dialogButton = G_OBJECT(dialog);
    g_signal_connect(dialogButton, "response", G_CALLBACK(dialogInput), GINT_TO_POINTER(GTK_RESPONSE_ACCEPT));

    // auto zoom
   std::pair<ezgl::point2d,ezgl::point2d> max_min = findMaxMinPoint(highlighted_route);
   ezgl::rectangle zoom(max_min.second, max_min.first);
   ezgl::renderer *h = application->get_renderer();
   ezgl::rectangle current_world = h->get_visible_world();

   int local_zoom = 0;
    // get the coordinates of the current world view rectangle

    double x_zoom_world =(current_world.m_first.x - current_world.m_second.x);
    double y_zoom_world =(current_world.m_first.y - current_world.m_second.y);


    double x_zoom_route = (zoom.m_first.x - zoom.m_second.x);
    double y_zoom_route = (zoom.m_first.y - zoom.m_second.y);

    double zoom_route = std::max(x_zoom_route,y_zoom_route);
    double zoom_world = std::max(y_zoom_world,x_zoom_world);

    double scale_x = zoom_route/zoom_world;

    if(scale_x > 25) {
        current_zoom_level =2;
        local_zoom=0;
    }
    else if(scale_x > 8) {
        current_zoom_level = 1;
        double scale_step = scale_x * 0.8;
        while (scale_x > 0) {
            local_zoom++;
            scale_x -= scale_step;
        }
    }
    else if(scale_x>3) {
        double scale_step = scale_x * 0.7;
        while (scale_x > 0) {
            local_zoom--;
            scale_x -= scale_step;
        }
    }
   else if (scale_x > 1) {
        double scale_step = scale_x * 0.5;
        while (scale_x > 0) {
            local_zoom--;
            scale_x -= scale_step;
        }
    }
    else if(scale_x > 0.5) {
        double scale_step = scale_x *0.7;
        while (scale_x > 0) {
            local_zoom++;
            scale_x -= scale_step;
        }
    }
    else if(scale_x < 0.15) {
        current_zoom_level -= scale_x*40;
        double scale_step = scale_x * 0.3;
        while (scale_x > 0) {
            local_zoom++;
            scale_x -= scale_step;
        }
    }
    else if(scale_x <0.05) {
        current_zoom_level = 1;
        double scale_step = scale_x * 0.3;
        while (scale_x > 0) {
            local_zoom++;
            scale_x -= scale_step;
        }
    }
    else {
        current_zoom_level -= scale_x*20;
        double scale_step = scale_x * 0.2;
        while (scale_x > 0) {
            local_zoom++;
            scale_x -= scale_step;
        }
    }

    current_zoom_level += local_zoom;

    h->set_visible_world(zoom);
    drawRoadArrows(highlighted_route,current_zoom_level,origin_intersection.first);
    application->refresh_drawing();
}

void dialogInput(GtkWidget* dialog ,ezgl::application* /*application*/, gpointer input){
    gint response = GPOINTER_TO_INT(input);
    if(response == GTK_RESPONSE_ACCEPT){
        gtk_widget_destroy(dialog);
        clearRoadArrows(highlighted_route);
        highlighted_route.clear();
   }
}

void searchRouteToggle(GtkToggleButton* search_route_toggle, ezgl::application* application){
    search_route = gtk_toggle_button_get_active(search_route_toggle);
    clearAllHighlights(application);
}


void initial_setup(ezgl::application *application, bool /*new_window*/) {
    application->update_message("Team 20 - ECE297");

    #ifdef VISUALIZE
    global_access = application;
    #endif

    // setting our starting row for insertion at 6 (default buttons take up first five rows)
    // increment row each time we insert a new element.
    int row = 6;
    application->create_popup_message("Complete", "All Items Drawn");
    application->update_message(std::to_string(current_zoom_level));
    ++row;

    // connect widges to callbacks
    GObject* origin_search_bar = application->get_object("OriginSearch");
    g_signal_connect(origin_search_bar, "activate", G_CALLBACK(searchEntryEnter), application);
    g_signal_connect(origin_search_bar, "changed", G_CALLBACK(searchEntryType), application);

    GObject* dest_search_bar = application->get_object("DestinationSearch");
    g_signal_connect(dest_search_bar, "activate", G_CALLBACK(searchEntryEnter), application);
    g_signal_connect(dest_search_bar, "changed", G_CALLBACK(searchEntryType), application);

    GObject* zoom_fit_button = application->get_object("ZoomFitButton");
    g_signal_connect(zoom_fit_button, "clicked", G_CALLBACK(zoomFit), application);

    GObject* maps_dropdown = application->get_object("city_maps");
    g_signal_connect(maps_dropdown, "changed",G_CALLBACK(change_map), application);

    GObject* ent_check = application->get_object("entertainmentselect");
    g_signal_connect(ent_check,"clicked",G_CALLBACK(draw_ent),application);

    GObject* trans_check = application->get_object("transportselect");
    g_signal_connect(trans_check,"clicked",G_CALLBACK(draw_trans),application);

    GObject* basic_check = application->get_object("servicesselect");
    g_signal_connect(basic_check,"clicked",G_CALLBACK(draw_basic),application);

    GObject* dark_mode_button = application->get_object("DarkModeButton");
    g_signal_connect(dark_mode_button, "clicked", G_CALLBACK(darkMode), application);

    GObject* search_route_toggle = application->get_object("SearchRouteToggle");
    g_signal_connect(search_route_toggle, "toggled", G_CALLBACK(searchRouteToggle), application);

    GObject* about_bttn = application->get_object("Aboutbtn");
    g_signal_connect(about_bttn, "activate", G_CALLBACK(aboutButton), application);

    GObject* help_bttn = application->get_object("Helpbtn");
    g_signal_connect(help_bttn, "activate", G_CALLBACK(helpButton), application);
}



void actOnMouseClick(ezgl::application* application, GdkEventButton* event, double x, double y) {

    // save previous state of origin_intersection
    bool origin_highlighted = globals.all_intersections[origin_intersection.first].highlight;

    clearAllHighlights(application);

    // right click
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        return;
    }

    bool select_poi_food = false;
    bool select_poi_shops = false;

    // find intersection selected
    LatLon selected_pos = LatLon(y_to_lat(y), x_to_lon(x));
    IntersectionIdx selected_intersection = findClosestIntersection(selected_pos);
    LatLon intersection_pos = getIntersectionPosition(selected_intersection);
    LatLon closest = LatLon(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    LatLon closest2 = LatLon(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    // find the closest featured POI
    if (globals.city_restaurants.size() > 0) {
        closest = globals.city_restaurants[0].pos;
    }
    if (globals.city_shops.size() > 0) {
        closest2 = globals.city_shops[0].pos;
    }
    double dist = findDistanceBetweenTwoPoints(selected_pos, closest);
    double dist2 = findDistanceBetweenTwoPoints(selected_pos, closest2);
    int index = 0;
    int index2 = 0;
    for (uint i = 0; i < globals.city_restaurants.size(); ++i) {
        double new_dist = findDistanceBetweenTwoPoints(selected_pos, globals.city_restaurants[i].pos);
        if (new_dist < dist) {
            dist = new_dist;
            index = i;
        }
    }
    for (uint i = 0; i < globals.city_shops.size(); ++i) {
        double new_dist = findDistanceBetweenTwoPoints(selected_pos, globals.city_shops[i].pos);
        if (new_dist < dist2) {
            dist2 = new_dist;
            index2 = i;
        }
    }
    if (dist <= dist2 && dist < 150) {
        select_poi_food = true;
    }
    else if (dist2 < 150) {
        select_poi_shops = true;
    }

    std::string message;
    if (!select_poi_food && !select_poi_shops) {

        // do not show popup and highlight if intersection name contains unknown
        if (getIntersectionName(selected_intersection).find("<unknown>") != std::string::npos) {

            // keep origin highlighted if origin is already highlighted and destination is not
            if (origin_highlighted && !set_origin){
                globals.all_intersections[origin_intersection.first].highlight = true;
            }
            application->refresh_drawing();
            return;
        }

        globals.all_intersections[selected_intersection].highlight = true;

        // do not show popup in search_route mode
        if (search_route){
            if (set_origin){
                origin_intersection.first = selected_intersection;
                origin_intersection.second = ezgl::point2d(x, y);
                set_origin = false;
            }
            else{
                destination_intersection.first = selected_intersection;
                destination_intersection.second = ezgl::point2d(x, y);
                outputRoad(application);
                set_origin = true;

                globals.all_intersections[origin_intersection.first].highlight = true;

            }
            application->refresh_drawing();
            return;
        }
        message += "Intersection Name: " + getIntersectionName(selected_intersection) + "\n";
        message += "Longitude: " + std::to_string(intersection_pos.longitude()) + "\n";
        message += "Latitude: " + std::to_string(intersection_pos.latitude()) + "\n";
        message += "ID: " + std::to_string(selected_intersection);
        application->create_popup_message("Intersection Information", message.c_str());
        clicked_intersection.first = selected_intersection;
        clicked_intersection.second = globals.all_intersections[selected_intersection].position;

    }
    else if (select_poi_food) {
        const char *title = globals.city_restaurants[index].poi_name.c_str();
        std::string message2;
        message2 += globals.city_restaurants[index].address + "\n";
        message2 += globals.city_restaurants[index].city + ", " + globals.city_restaurants[0].country + "\n";
        message2 += globals.city_restaurants[index].inner_category + "\n";
        message2 += "Rating: " + std::to_string((static_cast<int>(globals.city_restaurants[index].rating*10)/10)) + "/10\n";
        message2 += globals.city_restaurants[index].website + "\n";
        message2 += "Copyright 2024 Foursquare";
        message2 += "\n";
        application->create_popup_message(title, message2.c_str());
    }
    else if (select_poi_shops) {
        const char *title = globals.city_shops[index2].poi_name.c_str();
        std::string message2;
        message2 += globals.city_shops[index2].address + "\n";
        message2 += globals.city_shops[index2].city + ", " + globals.city_shops[0].country + "\n";
        message2 += globals.city_shops[index2].inner_category + "\n";
        message2 += "Rating: " + std::to_string((static_cast<int>(globals.city_shops[index2].rating*10)/10)) + "/10\n";
        message2 += globals.city_shops[index2].website + "\n";
        message2 += "Copyright 2024 Foursquare";
        message2 += "\n";
        application->create_popup_message(title, message2.c_str());
    }

    application->refresh_drawing();
}


// Main function, called from main.cpp
void drawMap() {
    ezgl::application::settings settings;
    settings.main_ui_resource = "libstreetmap/resources/main.ui";
    settings.window_identifier = "MainWindow";
    settings.canvas_identifier = "MainCanvas";
    ezgl::application application(settings);
    ezgl::rectangle initial_world({lon_to_x(globals.min_lon), lat_to_y(globals.min_lat)}, {lon_to_x(globals.max_lon), lat_to_y(globals.max_lat)});

    application.add_canvas("MainCanvas", draw_main_canvas, initial_world);

    application.run(initial_setup, actOnMouseClick, nullptr, nullptr);
}



void draw_main_canvas(ezgl::renderer *g) {
    if (globals.dark_mode){
        ezgl::rectangle visible_world = g->get_visible_world();
        g->set_color(53, 59, 66, 255);
        g->fill_rectangle(visible_world);
    }

    get_current_zoom_level(x_zoom_prev, y_zoom_prev, current_zoom_level, g);

    draw_features(g);

    way_draw_features(g);

    drawStreets(g);

    highlightRoute(g,highlighted_route);

    redrawStreetComponents(g,highlighted_route);

    drawHighlightedIntersections(g);

    drawPOIPng(g);

    std::vector<DeliveryInf> deliveries;
    std::vector<IntersectionIdx> depots;
    std::vector<CourierSubPath> result_path;

    deliveries = {DeliveryInf(171961, 41792), DeliveryInf(145152, 151088), DeliveryInf(96058, 57692), DeliveryInf(173265, 66308), DeliveryInf(52222, 48968), DeliveryInf(131184, 61922), DeliveryInf(110624, 130404), DeliveryInf(44815, 103959), DeliveryInf(105931, 111575), DeliveryInf(54071, 111889), DeliveryInf(34725, 105882), DeliveryInf(153323, 71800), DeliveryInf(37717, 36213), DeliveryInf(174923, 92754), DeliveryInf(83337, 123016), DeliveryInf(191528, 179887), DeliveryInf(104028, 161047), DeliveryInf(135783, 47419), DeliveryInf(132416, 171471), DeliveryInf(175707, 157213), DeliveryInf(150803, 105756), DeliveryInf(122449, 155253), DeliveryInf(186424, 13106), DeliveryInf(156304, 54603), DeliveryInf(136971, 140068)};
    depots = {13, 49589};
    std::vector<StreetSegmentIdx> to_draw = {147, 15925, 6, 108263, 116679, 258078, 565, 116686, 12403, 116692, 116691, 157956, 12413, 147719, 83481, 147556, 191222, 161855, 161852, 161853, 161854, 12398, 12399, 160750, 160751, 160752, 160753, 160272, 168759, 168760, 168761, 60736, 160146, 160147, 60741, 60733, 60734, 142755, 60743, 60744, 142753, 142754, 191586, 191585, 191588, 161369, 60635, 60634, 60633, 60632, 60622, 60629, 60631, 206575, 206576, 89892, 89893, 159677, 159676, 159678, 159679, 159680, 159681, 159682, 159683, 159684, 237148, 237148, 159685, 159686, 159687, 159688, 159689, 159690, 159691, 159692, 159693, 159694, 159695, 159696, 159697, 159698, 159699, 159700, 186094, 186068, 85207, 85208, 85209, 85210, 85211, 85212, 85213, 194116, 194117, 194118, 194119, 194120, 194121, 194122, 60901, 60897, 60892, 60893, 60894, 60902, 60903, 60904, 60905, 60906, 60907, 60908, 60909, 60910, 60911, 60912, 60913, 60914, 60915, 142092, 142097, 142098, 142099, 142100, 142101, 142102, 60938, 60939, 60933, 60945, 60946, 60950, 60951, 60952, 60953, 60954, 60955, 60956, 60957, 60970, 60971, 60972, 60974, 60966, 60963, 60979, 60980, 60981, 60982, 60983, 136092, 136087, 136088, 136089, 136090, 136091, 28465, 28466, 145376, 145377, 145371, 145372, 145373, 145374, 145375, 145368, 15845, 28152, 28153, 28154, 28155, 28156, 28157, 28118, 28119, 28120, 28121, 28122, 101362, 101362, 28123, 28124, 135168, 135169, 135183, 131139, 43102, 3939, 149910, 423, 149909, 15941, 16915, 117399, 117398, 117401, 146220, 274738, 146221, 166003, 166007, 136376, 136377, 136375, 42387, 131165, 221268, 131166, 131167, 131168, 131169, 22049, 22050, 22051, 103144, 103142, 136118, 136120, 136119, 29137, 150410, 118537, 29595, 101289, 101288, 101292, 101291, 101290, 142637, 142636, 29147, 117221, 117220, 117219, 118813, 118812, 118811, 118810, 118809, 142633, 142632, 142631, 142630, 246466, 246465, 130104, 183687, 184262, 130109, 183688, 183686, 118191, 157317, 157318, 157319, 180821, 180822, 39748, 39749, 39750, 39751, 183685, 166417, 189159, 183280, 180817, 180818, 180819, 117528, 180820, 7765, 141776, 141777, 191590, 191595, 191596, 191597, 219209, 87951, 87758, 87757, 87952, 7762, 118689, 8908, 8907, 8906, 140747, 140748, 140750, 140749, 131316, 131315, 131314, 131313, 131312, 131311, 254459, 254458, 117030, 117033, 117032, 117031, 128720, 128719, 128718, 128717, 131318, 131317, 147901, 131319, 117124, 117123, 117122, 87123, 87789, 87790, 168651, 168650, 168649, 161238, 222906, 171995, 171994, 171993, 171992, 171997, 171996, 18885, 18884, 87854, 18886, 87122, 87121, 87120, 87119, 87118, 87117, 243, 83422, 83421, 83420, 83419, 83418, 83417, 83414, 144516, 144517, 140650, 140651, 140652, 140653, 140654, 140655, 140656, 140657, 140658, 140659, 140660, 140661, 218660, 60, 61, 62, 63, 64, 65, 151167, 88283, 88281, 88284, 88282, 172323, 88287, 88288, 88289, 88290, 88297, 88298, 88299, 88302, 88309, 88310, 277977, 144499, 88307, 144839, 144840, 144841, 144842, 144843, 144844, 144845, 144846, 5341, 5342, 68300, 68296, 68297, 68298, 68299, 118685, 118685, 68299, 68298, 68297, 68296, 68300, 140421, 140420, 88264, 88263, 88262, 118661, 118662, 118663, 3695, 85350, 85347, 85346, 106082, 85332, 145465, 145468, 145470, 145467, 9143, 28661, 116798, 240611, 28659, 28660, 116802, 107362, 128737, 132419, 192427, 11113, 11114, 192428, 192426, 116807, 116809, 23367, 23368, 132428, 132429, 132427, 563, 106074, 23398, 23399, 106077, 106078, 106079, 28293, 28294, 28303, 179099, 17631, 116825, 17635, 20863, 168721, 168720, 30153, 20878, 20879, 168722, 17648, 17647, 69645, 163336, 163333, 163332, 163335, 163334, 163331, 163328, 163329, 163330, 48065, 48064, 159123, 159122, 159121, 159125, 258056, 159124, 30413, 30412, 30411, 173315, 173314, 65839, 65862, 65851, 65852, 65847, 201608, 201608, 65847, 65524, 65512, 65493, 65492, 65874, 29791, 29792, 88740, 150147, 54700, 54701, 54702, 54703, 54704, 54705, 54706, 54707, 54708, 54709, 147132, 147133, 147134, 147135, 147136, 147137, 193504, 193486, 193488, 193489, 193485, 12003, 12004, 216590, 220689, 12008, 12009, 8656, 12030, 12029, 12021, 87455, 87456, 116849, 263289, 116850, 5772, 13352, 13357, 13360, 13358, 13359, 128639, 5773, 557, 5700, 185359, 118937, 118936, 118935, 118934, 185357, 185352, 222981, 176671, 176669, 410, 179830, 176679, 179829, 190294, 134040, 165892, 408, 248801, 29164, 216129, 27067, 119974, 119975, 119978, 119979, 119980, 119981, 119982, 119983, 119984, 119985, 119986, 176915, 176918, 215952, 119594, 119595, 119596, 119597, 119598, 119599, 119600, 119601, 119602, 119603, 119604, 119605, 119606, 119607, 119608, 176896, 176897, 176895, 119995, 119994, 119993, 119992, 119991, 119990, 119989, 119988, 119987, 120004, 120005, 120006, 120007, 164625, 164624, 164624, 164625, 120007, 120006, 120005, 120004, 120003, 120002, 120001, 120000, 119999, 119998, 119997, 119996, 190291, 177960, 177954, 159947, 159948, 159949, 216618, 119480, 46873, 54755, 46874, 46875, 178006, 178007, 189107, 189108, 216523, 178004, 178005, 121705, 121706, 121707, 121708, 121709, 121710, 121711, 121712, 90440, 235438, 189084, 189085, 235435, 90436, 90437, 90438, 235415, 235414, 235416, 235417, 235379, 235380, 235378, 235377, 235376, 215964, 215965, 235369, 235370, 235371, 235373, 235374, 235375, 235348, 190230, 190228, 190232, 190233, 190234, 165981, 165982, 47437, 165882, 165883, 180558, 180553, 26792, 180556, 180557, 180542, 190237, 180560, 180561, 180562, 180563, 180603, 180559, 180564, 180575, 190210, 180569, 180573, 180574, 180571, 180570, 180572, 180594, 214375, 401, 402, 403, 180592, 190214, 190216, 169115, 169116, 169117, 169118, 169119, 169120, 235927, 169114, 133961, 235925, 235924, 235922, 235919, 172622, 172623, 172624, 190338, 190341, 26790, 235915, 235914, 141878, 189313, 189314, 235912, 189794, 141874, 141875, 141876, 119521, 26793, 26794, 26795, 235906, 189326, 235905, 189324, 189325, 189318, 189319, 189317, 189320, 235903, 174764, 174765, 115179, 115182, 115183, 243589, 115172, 243590, 115174, 115175, 115164, 243591, 115166, 214936, 115149, 115151, 214203, 115165, 124422, 243596, 243595, 180444, 115146, 115147, 115148, 115152, 115153, 115154, 115155, 115156, 115157, 115158, 92962, 92963, 92921, 92965, 92966, 243597, 92969, 92973, 92970, 243599, 243598, 142240, 142239, 142238, 142199, 142200, 142201, 189600, 189601, 142226, 142202, 142203, 142204, 142205, 142206, 142207, 142208, 142209, 142210, 142211, 142212, 142213, 142214, 142215, 142216, 142217, 142218, 142219, 142220, 142221, 142222, 142223, 92834, 92825, 92831, 92828, 92767, 92768, 261404, 261404, 92768, 92767, 92830, 92827, 92825, 92829, 41472, 41473, 41474, 41475, 166422, 166423, 166424, 166425, 166426, 166427, 92811, 159016, 159016, 92812, 92813, 92814, 92815, 92816, 214055, 214054, 214053, 214052, 214051, 214050, 91763, 256367, 256368, 256369, 256370, 256371, 256372, 256373, 256374, 256375, 256376, 256377, 256378, 256379, 256380, 256381, 256382, 256383, 193297, 193122, 276696, 193119, 276701, 193118, 170783, 170784, 244983, 244984, 276714, 276715, 276717, 276718, 276719, 276720, 276721, 244985, 244982, 276722, 276727, 163188, 163189, 276733, 276734, 276730, 13430, 13435, 13427, 85909, 85910, 34874, 85912, 85913, 85911, 85916, 85917, 85919, 85918, 91408, 91409, 85920, 242492, 85921, 117582, 249772, 249773, 107479, 249770, 85899, 249775, 249780, 36392, 95576, 95577, 12180, 12181, 85922, 85390, 121785, 160256, 66540, 66141, 66062, 66423, 66424, 256743, 85395, 44771, 44628, 44629, 44630, 44631, 44632, 44633, 44634, 44635, 44730, 44731, 44732, 211435, 211423, 211446, 44972, 44971, 44971, 211729, 211714, 211732, 45292, 45293, 44325, 44326, 44327, 44328, 45703, 45704, 45705, 45706, 45707, 45708, 45709, 45710, 45711, 45712, 45713, 45714, 45715, 45716, 45717, 45718, 45719, 45720, 45721, 209787, 209783, 209785, 44616, 44617, 251821, 273008, 273003, 252026, 252027, 252028, 273005, 46017, 46018, 45691, 273021, 173480, 173481, 173482, 256939, 252030, 45584, 45499, 163132, 158633, 158634, 45065, 232394, 268053, 268054, 268055, 268056, 247362, 247363, 247367, 122152, 196462, 196461, 196461, 196462, 122152, 247367, 247363, 247362, 268057, 252034, 252031, 252032, 1520, 122139, 140761, 249882, 249878, 249885, 249886, 242485, 162432, 162433, 162436, 19720, 231813, 107358, 9470, 91379, 240435, 250689, 91380, 91381, 91382, 91383, 124683, 96325, 96273, 219978, 96342, 96343, 96280, 96281, 96294, 96298, 96284, 96079, 96062, 96164, 96439, 96440, 96441, 96442, 96401, 96402, 96403, 96476, 155003, 96394, 96455, 96456, 96457, 96458, 96459, 96460, 230763, 230765, 230765, 230763, 96461, 96462, 96463, 96464, 96465, 256397, 256396, 132778, 96479, 256398, 256399, 256400, 256401, 256402, 234088, 234089, 234090, 234091, 234105, 234104, 234104, 234106, 234108, 255836, 255842, 255841, 256402, 256401, 256400, 256399, 256398, 96479, 230832, 240688, 240689, 256105, 256106, 256107, 256108, 265459, 256109, 265460, 96467, 96468, 96469, 96470, 96471, 240684, 250706, 126965, 126966, 126968, 126970, 126972, 250709, 250710, 250711, 250712, 250713, 127005, 127006, 127010, 218906, 126999, 126995, 116253, 249571, 249581, 249582, 249583, 249585, 249584, 249587, 249590, 170166, 170165, 215638, 249591, 155120, 155119, 155118, 155117, 155116, 155115, 155114, 155113, 155270, 155268, 155267, 231741, 231741, 155266, 155269, 155271, 155272, 243312, 98887, 98911, 98912, 98913, 98914, 98915, 133249, 133250, 241356, 234712, 241353, 241354, 234704, 234703, 234706, 133279, 133280, 234705, 253262, 253260, 253261, 234709, 234710, 234711, 251238, 95460, 52109, 52110, 52111, 223171, 204104, 204105, 204094, 204095, 223169, 53765, 53764, 53763, 53762, 53899, 50890, 53058, 50279, 53244, 51228, 51227, 51226, 51225, 51224, 51223, 223164, 51548, 223163, 50417, 50416, 50415, 50912, 131371, 131370, 23081, 132438, 132439, 132436, 116243, 128015, 215918, 215917, 215916, 215915, 215920, 215919, 132437, 102268, 256688, 256687, 234903, 100761, 100762, 250439, 256689, 256700, 256701, 266775, 266776, 168505, 168504, 168503, 266777, 266778, 156101, 266774, 266780, 266771, 175296, 147860, 147863, 147864, 147861, 156100, 100627, 99074, 99075, 99079, 99080, 245781, 245780, 245779, 245782, 245783, 128140, 128139, 128138, 128137, 128136, 128141, 75876, 75858, 75859, 75857, 75860, 75872, 75873, 75874, 75875, 240757, 240758, 240759, 240760, 134873, 134872, 134871, 116271, 250605, 187079, 116270, 116266, 116267, 116272, 134640, 110557, 110552, 110553, 110554, 250779, 250781, 110549, 110550, 110551, 252227, 110561, 110559, 110560, 110564, 246428, 215639, 246427, 250783, 250784, 250785, 250786, 250788, 250789, 250790, 250791, 250792, 250793, 250794, 250795, 248254, 255555, 134829, 248255, 250797, 250796, 248266, 255554, 250798, 248267, 248278, 222431, 222432, 248277, 76038, 76039, 150477, 150475, 150476, 76560, 76561, 76562, 76556, 76567, 76568, 248286, 215578, 248285, 250800, 215715, 250799, 248290, 215716, 215717, 248289, 100396, 250802, 250803, 188172, 223363, 100490, 100782, 100684, 100685, 100687, 250804, 100735, 250805, 250806, 100362, 100363, 100364, 250807, 250808, 250809, 250810, 250811, 250814, 100849, 142849, 142848, 142847, 142846, 142845, 100599, 100445, 100446, 100447, 100448, 151776, 100789, 100790, 100791, 100238, 23128, 23129, 23130, 23131, 23132, 23133, 74476, 135770, 74488, 23133, 23134, 23135, 23136, 212557, 212556, 162347, 162346, 212555, 212554, 139144, 139145, 135760, 135761, 135762, 135763, 215697, 215698, 215699, 254176, 254177, 74962, 74963, 196895, 196896, 196897, 196898, 196888, 196899, 196900, 196901, 196894, 197053, 197049, 197051, 197050, 197052, 196917, 196914, 196918, 196919, 196915, 196916, 215646, 183912, 184114, 183913, 184113, 100324, 100756, 248183, 183914, 129973, 163505, 100410, 183905, 100487, 100383, 108283, 108284, 168453, 168452, 108289, 108288, 108287, 108286, 108285, 108303, 108304, 108305, 108309, 270725, 26696, 250524, 250525, 130246, 108298, 108299, 91197, 91198, 91191, 91192, 91193, 91194, 91195, 91196, 133687, 121805, 121806, 113346, 131221, 162404, 247358, 247359, 176199, 184990, 184997, 184999, 217467, 216875, 216874, 184992, 184991, 162403, 162402, 184987, 170688, 185005, 166495, 74607, 74610, 74605, 74604, 108348, 108349, 172089, 172088, 130751, 130750, 185199, 185192, 185190, 185191, 185193, 185189, 74615, 74614, 108350, 179825, 108486, 244053, 179823, 215330, 215329, 187664, 238431, 238432, 238433, 187666, 187667, 187668, 187669, 187670, 187671, 187672, 187673, 187674, 187675, 187676, 187677, 187649, 179819, 179820, 108482, 108489, 108485, 108352, 108351, 195496, 195495, 195494, 195501, 195498, 182331, 182330, 182333, 182329, 182334, 182336, 156726, 185145, 185144, 156725, 190408, 190409, 215237, 190406, 190405, 215309, 51144, 106282, 50034, 50033, 50032, 50031, 50030, 50029, 191691, 191692, 218359, 218360, 197084, 197085, 197082, 197083, 278046, 191685, 62734, 191307, 62867, 191308, 191309, 167415, 167416, 167417, 156723, 156723, 167417, 167416, 167415, 191309, 191308, 62867, 191307, 62734, 191685, 278045, 278044, 191686, 191687, 278047, 278048, 278049, 278050, 278051, 197087, 197086, 278055, 278052, 278053, 278057, 278058, 278059, 53825, 53941, 54409, 52659, 225782, 180722, 52903, 225908, 49242, 182303, 186456, 186457, 50431, 50432, 108665, 108653, 108654, 53798, 53797, 53796, 53795, 53794, 53793, 53705, 52233, 52084, 52083, 53312, 50606, 52434, 54029, 52867, 53008, 53504, 50684, 53279, 50409, 90397, 52038, 52947, 218395, 53007, 51506, 51199, 49989, 51846, 62701, 62702, 62703, 49563, 49564, 198277, 52275, 52276, 198251, 198252, 48941, 198255, 51618, 215403, 51979, 215428, 215427, 215395, 54567, 49633, 50414, 227342, 51293, 227343, 51719, 53579, 62589, 62588, 50792, 52555, 49983, 54402, 54402, 62633, 51648, 51593, 52960, 53390, 50602, 62591, 263925, 62621, 52489, 53338, 62604, 112238, 112237, 263923, 215338, 108545, 108544, 108543, 108542, 108541, 108540, 242769, 242768, 242767, 172998, 172997, 172996, 172995, 172994, 172993, 172992, 172991, 218357, 218356, 218355, 218354, 256779, 112284, 112283, 156470, 156469, 156468, 156467, 90709, 108555, 108554, 48490, 189058, 189057, 189056, 189055, 189054, 189051, 189053, 189052, 189062, 189061, 189060, 189059, 192569, 192568, 192567, 159612, 276554, 276553, 62696, 62693, 62692, 62690, 48489, 48488, 185695, 185697, 185696, 185693, 48518, 48517, 48516, 215186, 215185, 215184, 215183, 17398, 17397, 91996, 156772, 53868, 160265, 50148, 53866, 53867, 49287, 49286, 49285, 49284, 49283, 49282, 49281, 52706, 52705, 52704, 49569, 49935, 49568, 54294, 53451, 53908, 53090, 53755, 53754, 53753, 49313, 53509, 49314, 49315, 254699, 254700, 215197, 215198, 215199, 215200, 169785, 169786, 169784, 169783, 270221, 9289, 2111, 2112, 178202, 29392, 29393, 148923, 148924, 62244, 62243, 62247, 62248, 18152, 18153, 18151, 178199, 53954, 62163, 29382, 29381, 29380, 29379, 29378, 29377, 29376, 29375, 255604, 73408, 118955, 118954, 118953, 118952, 118951, 118950, 118949, 118949, 118950, 118951, 118952, 118953, 118954, 118955, 73408, 255604, 29375, 29376, 29377, 29378, 29379, 29380, 29381, 29382, 62164, 62165, 62166, 9430, 178200, 118854, 2121, 18156, 108851, 108841, 29296, 9415, 118848, 129228, 108843, 118847, 108862, 12428, 263310, 108866, 19184, 108887, 157313, 12425, 157312, 157316, 108888, 3732, 6178, 144800, 154004, 72288, 72286, 277844, 144801, 144802, 72289, 72291, 151254, 72294, 168525, 168523, 168524, 72293, 277858, 151250, 151251, 221398, 59049, 59050, 59047, 59048, 59046, 277857, 277856, 28905, 28906, 183851, 183850, 183853, 183854, 108921, 183852, 183849, 183848, 183847, 183846, 108923, 36063, 36064, 181097, 181097, 180427, 180428, 108924, 108925, 186147, 186148, 186149, 6512, 180429, 134171, 134172, 36065, 36066, 183973, 183974, 183975, 183976, 183977, 183978, 183979, 92003, 183971, 277486, 183980, 183970, 180380, 180381, 180379, 88500, 88505, 88496, 88502, 88511, 88510, 87424, 87418, 87419, 87420, 87421, 87422, 87423, 163407, 277483, 163409, 163405, 163406, 57722, 57723, 57725, 120922, 277481, 153293, 120926, 120927, 120928, 120929, 120930, 120931, 120932, 120933, 120934, 120962, 120963, 277477, 172125, 120916, 120917, 120918, 120919, 120920, 120921, 254930, 277474, 254929, 63436, 63437, 63438, 63439, 63440, 63446, 63448, 180366, 180367, 63454, 63455, 63451, 277469, 63447, 63456, 63457, 63458, 63459, 63462, 63463, 63460, 63465, 63466, 180808, 180809, 180811, 180810, 123005, 123006, 123007, 123008, 123009, 159806, 159807, 159808, 159809, 148083, 148084, 148085, 160254, 160255, 160248, 160249, 160250, 160251, 160252, 160253, 161847, 161848, 161849, 161850, 106758, 106759, 106760, 125745, 125746, 125747, 125748, 125749, 125750, 125751, 125752, 125753, 125772, 125772, 125771, 125770, 125769, 125768, 125791, 125792, 125793, 106763, 246101, 143028, 143027, 143026, 143025, 143024, 143023, 143022, 143021, 143020, 143019, 143018, 122974, 122973, 104081, 122971, 148416, 148415, 122865, 191224, 122867, 185503, 107970, 180361, 180360, 142316, 89089, 151990, 184290, 89106, 184293, 184294, 184289, 89095, 89096, 89097, 89099, 89094, 113082, 113081, 113080, 113087, 118168, 191274, 191273, 191272, 192376, 191271, 191275, 192531, 191279, 191278, 191277, 161856, 160241, 160240, 160240, 160241, 161856, 191277, 191278, 191279, 192531, 191275, 191270, 192377, 192378, 22680, 22679, 22678, 22677, 107560, 107584, 107583, 107582, 107580, 107579, 107578, 107577, 107576, 107574, 107575, 168779, 168778, 168777, 168781, 168780, 22676, 191392, 168776, 191391, 197539, 168784, 168783, 103122, 103121, 103120, 103119, 103118, 103117, 103116, 103115, 103114, 103113, 103112, 190840, 190843, 190842, 190841, 186861, 186860, 103109, 103106, 186859, 186858, 89808, 89807, 89806, 89805, 103131, 190670, 153549, 153548, 32584, 889, 888, 59366, 186126, 190663, 190664, 190665, 190666, 59368, 59369, 59370, 59371, 59372, 59373, 59374, 59375, 58399, 58401, 58402, 58403, 58404, 58405, 58406, 58407, 58408, 58409, 58410, 58411, 58393, 58392, 58396, 117685, 153749, 153750, 153751, 153752, 153753, 153754, 153755, 180425, 153760, 153761, 70487, 153756, 70485, 70486, 180413, 180414, 180415, 180416, 180417, 180418, 180408, 180406, 70499, 70500, 70501, 70502, 180407, 180411, 180412, 180409, 180410, 180404, 48179, 180400, 48180, 180405, 180399, 180401, 180402, 180403, 82246, 82247, 82248, 82249, 82250, 82251, 180398, 82259, 82263, 82260, 82265, 82266, 82267, 82268, 82269, 82270, 82271, 82272, 82273, 118401, 118400, 118403, 118404, 118405, 118406, 118407, 118408, 118409, 118410, 118411, 118412, 118413, 118415, 118416, 254358, 118419, 118420, 118421, 118422, 118423, 118424, 254359, 254360, 254361, 131510, 131511, 131512, 184074, 134174, 134175, 134176, 134177, 134178, 134179, 134180, 134181, 134182, 134183, 134184, 134185, 134186, 134187, 134188, 134189, 134190, 67177, 67178, 67179, 67180, 67181, 67100, 67101, 67102, 67103, 67104, 66993, 67173, 67174, 67175, 67176, 67172, 66977, 66978, 28099, 28100, 28101, 67238, 130235, 130236, 130237, 130238, 130239, 192349, 192349, 130239, 130238, 130237, 130236, 130235, 67238, 28101, 28100, 28099, 67092, 67091, 67090, 67089, 67088, 67087, 87805, 87804, 87803, 87802, 87801, 87800, 234141, 234140, 234139, 234138, 234137, 36036, 36035, 36034, 36033, 185980, 67288, 67292, 67290, 185979, 99210, 99211, 249911, 249912, 249910, 249913, 256331, 256332, 249914, 249916, 249919, 245901, 222984, 99569, 99237, 249929, 249930, 99724, 99585, 234919, 107475, 2879, 2878, 107477, 2888, 1528, 249787, 106090, 121786, 106069, 45664, 161817, 66424, 256743, 256744, 256745, 256749, 256751, 256750, 249823, 249822, 256752, 249825, 249826, 249827, 249828, 192640, 192639, 192638, 192637, 192636, 273130, 273133, 192635, 241192, 241191, 215588, 241193, 20400, 20399, 20398, 20397, 20396, 249693, 249696, 98501, 231744, 231745, 169020, 169021, 231746, 231747, 186488, 186487, 186489, 186491, 272956, 272960, 186483, 231806, 231805, 231802, 256880, 272968, 272967, 256878, 152118, 231803, 152116, 152117, 215560, 215559, 241320, 231800, 272920, 272918, 241322, 231801, 231799, 231709, 231708, 139084, 241328, 188551, 272980, 231707, 272971, 272972, 188556, 188553, 188569, 189218, 189217, 188565, 272524, 219035, 272520, 272520, 219035, 272521, 188568, 188552, 142108, 142110, 272745, 272746, 142113, 142111, 188555, 272748, 272750, 188554, 142107, 114600, 114599, 248187, 272883, 272884, 272878, 192403, 248186, 231715, 46283, 46284, 46285, 46286, 46287, 46288, 265412, 265413, 243402, 243370, 265409, 243373, 243374, 243377, 243398, 243397, 265210, 265211, 265212, 265214, 265213, 42451, 42451, 265209, 243394, 243395, 216651, 243378, 243400, 243379, 243375, 265408, 265410, 243376, 243371, 265426, 265427, 243372, 265445, 265446, 243361, 265450, 265452, 265455, 243362, 8019, 265456, 107485, 117029, 251005, 8017, 263277, 107476, 8004, 8005, 48318, 48314, 117318, 48308, 48307, 21071, 21074, 21075, 117325, 884, 140376, 103384, 103379, 103380, 103378, 170915, 73046, 170906, 73060, 73059, 73029, 73028, 73051, 73050, 73049, 73095, 73093, 73092, 221208, 29155, 29154, 155482, 155483, 29152, 29151, 29150, 180701, 180700, 216520, 29149, 29148, 56807, 56808, 56809, 56810, 56811, 56812, 56813, 56814, 56815, 56816, 216575, 216573, 216574, 154711, 187190, 187189, 106036, 48745, 192625, 192622, 189832, 187361, 165885, 189830, 14006, 187188, 197397, 197398, 197396, 187193, 187173, 2032, 2033, 91881, 91882, 91883, 91884, 199988, 199988, 91884, 91883, 91882, 91881, 2034, 2035, 2036, 90892, 170796, 189469, 170895, 189473, 170810, 170811, 170812, 238633, 238634, 214676, 94267, 238648, 238647, 27686, 27687, 27688, 170822, 170823, 170832, 276679, 193299, 276682, 193306, 193307, 238664, 238665, 193305, 170813, 256361, 238667, 238668, 238666, 193334, 193331, 193332, 193328, 193329, 193330, 163300, 163301, 163302, 163303, 170836, 170837, 170838, 170843, 189510, 189512, 170814, 170815, 170816, 170817, 238685, 238686, 189501, 191110, 189502, 189498, 170757, 170758, 170761, 170766, 100975, 189126, 236063, 189122, 189121, 189125, 189123, 189124, 27681, 27682, 170762, 170765, 189133, 170767, 189135, 30138, 30139, 170759, 170760, 170764, 184652, 170763, 189149, 25670, 25671, 25673, 25674, 236070, 236069, 181826, 181827, 181828, 181829, 181830, 181831, 236078, 119919, 119920, 119921, 119922, 119923, 119924, 119925, 119926, 119927, 119928, 119929, 119930, 119931, 119932, 119933, 119934, 119935, 119936, 119937, 119938, 119939, 119940, 119941, 119942, 119943, 236085, 236086, 236084, 236089, 236090, 236091, 92571, 92572, 92573, 92574, 92575, 92576, 92577, 92578, 92579, 92580, 236092, 241985, 241986, 241989, 241990, 236101, 25803, 25804, 256330, 241981, 106703, 106704, 106705, 236108, 172033, 172032, 34889, 34888, 236113, 34896, 236112, 102151, 102152, 192307, 192306, 33843, 33842, 33841, 33840, 33839, 33898, 131395, 133879, 133878, 133877, 133876, 34920, 34919, 33963, 33962, 33961, 34927, 34932, 34941, 220373, 220373, 34941, 34932, 34927, 34926, 34925, 34924, 34923, 34922, 34921, 189805, 189804, 189803, 189802, 189808, 214070, 194007, 194009, 152530, 152527, 46853, 194008, 91615, 91614, 91613, 91612, 91611, 189934, 189935, 189936, 189930, 189929, 190077, 214205, 243805, 1215, 243812, 243811, 243810, 214105, 214106, 134369, 134362, 243813, 190091, 190092, 189943, 54682, 106065, 193467, 193468, 189942, 243564, 136653, 93952, 191299, 191300, 191301, 191298, 108565, 108566, 108567, 108568, 108569, 108570, 108571, 108572, 108573, 108574, 214971, 214972, 214973, 214974, 214975, 214976, 214977, 214978, 176663, 176664, 176665, 215152, 139903, 47441, 47442, 176661, 176662, 139905, 139906, 187865, 119027, 176703, 176709, 176710, 143781, 143773, 143773, 143782, 143783, 115056, 128725, 115055, 217276, 187269, 187270, 124583, 227684, 176497, 227683, 176489, 176492, 176493, 176490, 176491, 227675, 176494, 176495, 176496, 176488, 177374, 124580, 124581, 191289, 119891, 191291, 119745, 227672, 119690, 37644, 37645, 37646, 37647, 105980, 105992, 105993, 227670, 119776, 119777, 119778, 119779, 119780, 136661, 191061, 26343, 26342, 26341, 26340, 26339, 26338, 39216, 39215, 39215, 39216, 168483, 39233, 39230, 39232, 39228, 39227, 148056, 39253, 39249, 39254, 39226, 39225, 39149, 187913, 102888, 102848, 102849, 102940, 102776, 102863, 102761, 102827, 102851, 102933, 190865, 102738, 220132, 102746, 220131, 190896, 102918, 102765, 102866, 102820, 102789, 102727, 102751, 102752, 102753, 102754, 102755, 102756, 102891, 102804, 102768, 102860, 278476, 278477, 278475, 278482, 278481, 278480, 102875, 102865, 102808, 102858, 102859, 186143, 102748, 102749, 102950, 102823, 188224, 102919, 102794, 102795, 273048, 102857, 102767, 269560, 175919, 273833, 33368, 33369, 33370, 33371, 273834, 175913, 175915, 175916, 175919, 269557, 102868, 102916, 216167, 102943, 102743, 102852, 102836, 102864, 102929, 264180, 264179, 102806, 102807, 102934, 102935, 102936, 102894, 102954, 102862, 102844, 216174, 263657, 107824, 24528, 278116, 24529, 121688, 121689, 121690, 188396, 188401, 188398, 104148, 104149, 188402, 188403, 188404, 109332, 109292, 107829, 188132, 188137, 108795, 108796, 108797, 108790, 188130, 188134, 104174, 188139, 188133, 188135, 108800, 188190, 188191, 109281, 188179, 188189, 188180, 109288, 109289, 188192, 188184, 188196, 188200, 188187, 124084, 216009, 192100, 192099, 100179, 31795, 31771, 31772, 31773, 31774, 31775, 31776, 31777, 31783, 31784, 31785, 31786, 31787, 31837, 31845, 31789, 228847, 100186, 100187, 100188, 188201, 188202, 188193, 24535, 24536, 165934, 165935, 17480, 12217, 24537, 81435, 139372, 137773, 129538, 129537, 129547, 129549, 129542, 23564, 23563, 129545, 129548, 129543, 1253, 8022, 129546, 33925, 131545, 33926, 33938, 171732, 141400, 175595, 172575, 172576, 188426, 221458, 221462, 188412, 33940, 216473, 216474, 216475, 216476, 216477, 188417, 188414, 98641, 23019, 188428, 188429, 188430, 104897, 104898, 216524, 216525, 216526, 216478, 166833, 216634, 216528, 216529, 104892, 104893, 104889, 188421, 188422, 188423, 175603, 188419, 188415, 175604, 175605, 175606, 221460, 221459, 172578, 172577, 33941, 156526, 141399, 33933, 131546, 27782, 30116, 30117, 141398, 47630, 107415, 107413, 129609, 129613, 22720, 22719, 129608, 129612, 47625, 47626, 119448, 129573, 11911, 19118, 19117, 19119, 19120, 165946, 140735, 140737, 140738, 140736, 47627, 47628, 141132, 141138, 141139, 141140, 141141, 141142, 157611, 157612, 22551, 22552, 22545, 22546, 22547, 22548, 22549, 22550, 157394, 157395, 157396, 140773, 140772, 140771, 140770, 138072, 138071, 138070, 138069, 138068, 138067, 138066, 138065, 138064, 138063, 138062, 138073, 277975, 138057, 157143, 141166, 141165, 136344, 136343, 136342, 136341, 172448, 172447, 172446, 172445, 172444, 172444, 172445, 172446, 172447, 172448, 136341, 136342, 136343, 136344, 141165, 141166, 138060, 141145, 138062, 138063, 138064, 138065, 138066, 138067, 138068, 138069, 138070, 138071, 138072, 140770, 140771, 140772, 140773, 10713, 10714, 172374, 8035, 8036, 116723, 46876, 46877, 8039, 22584, 22583, 29649, 29650, 10663, 120123, 120135, 227822, 227821, 29636, 224618, 224621, 224617, 29637, 29638, 29639, 251534, 251533, 227660, 227661, 12370, 12371, 12372, 12373, 227657, 227658, 105680, 54983, 229673, 227654, 131500, 229672, 229671, 229670, 229669, 48356, 48355, 48354, 48353, 48352, 48351, 119613, 119612, 119611, 43180, 43181, 43182, 43183, 43184, 43185, 43186, 43187, 131499, 259776, 230543, 259777, 252779, 252795, 252778, 256904, 256903, 256906, 230565, 221664, 145962, 6941, 6942, 6943, 6944, 6945, 6946, 119617, 28790, 188678, 188679, 188680, 188681, 188682, 188674, 188672, 188673, 188675, 188676, 188677, 188670, 188671, 28791, 28792, 28793, 28794, 28795, 28796, 28797, 28798, 28799, 28800, 28801, 67631, 67627, 67629, 241405, 67628, 67630, 67539, 67537, 188663, 188662, 188664, 188665, 67697, 67696, 67695, 272423, 272424, 183142, 144250, 144251, 144249, 69213, 69212, 69211, 69201, 231889, 231892, 54998, 54997, 54996, 88731, 88730, 88729, 69191, 69190, 69189, 69186, 54995, 54994, 54993, 141714, 141713, 141712, 141711, 141710, 141709, 141708, 141707, 256209, 161721, 161722, 161719, 12017, 12002, 12035, 12033, 188490, 188489, 188488, 116846, 116829, 250657, 116832, 87444, 87440, 116830, 116831, 249854, 28295, 28296, 116826, 28285, 28286, 28281, 28282, 150725, 150726, 106075, 116814, 23392, 23393, 116816, 116815, 6332, 132425, 132426, 116822, 116823, 23373, 23374, 116808, 11108, 15398, 15399, 11111, 11112, 11109, 106080, 248713, 20159, 248656, 248655, 248654, 248653, 248652, 248651, 248650, 248649, 248648, 248647, 248646, 248645, 20157, 20156, 131542, 131539, 226993, 226992, 131527, 226991, 142366, 15264, 278033, 118866, 278034, 251273, 251272, 118868, 226991, 142366, 15264, 278033, 131528, 183088, 15261, 15258, 116780, 116779, 233542, 233543, 233544, 249800, 30385, 268395, 87134, 190545, 190551, 121736, 190554, 190548, 190547, 226603, 226602, 190556, 190552, 196780, 191204, 191203, 197059, 197058, 217791, 142254, 87683, 87685, 190550, 127996, 190544, 54962, 54961, 156094, 27597, 197785, 153512, 87662, 115592, 115590, 115591, 115594, 115595, 163214, 184096, 85839, 148959, 85840, 85890, 87364, 27598, 39756, 103158, 39757, 39761, 39760, 90360, 90359, 90356, 256539, 256538, 148171, 90358, 90357, 184030, 155942, 183796, 183795, 183794, 183797, 183793, 194167, 194166, 194165, 194164, 220632, 242859, 220633, 194163, 138773, 138772, 138771, 138770, 149908, 138774, 114893, 114894, 138775, 256537, 256536, 156589, 156588, 156587, 256535, 256534, 256533, 156591, 156590, 156592, 15765, 15764, 15763, 15762, 15761, 955, 137211, 137212, 951, 952, 956, 957, 102418, 102419, 102420, 102416, 143673, 88278, 138664, 88279, 88280, 22042, 283, 148343, 148344, 138665, 148345, 19187, 148346, 148347, 148348, 148349, 148350, 148351, 3067, 142712, 142713, 142714, 142715, 142716, 142717, 142718, 11576, 11577, 11578, 11579, 11580, 11581, 11582, 183966, 1827, 47316, 47315, 47314, 2052, 181165, 181166, 181167, 181168, 1816, 1817, 1818, 1819, 1820, 1821, 1822, 1823, 2047, 19280, 14169, 14170, 19285, 2823, 159589, 9524, 9525, 9526, 9527, 9528, 9529, 32960, 32962, 190111, 190112, 190113, 190114, 204739, 190110, 32961, 194079, 194080, 194081, 243222, 169722, 190119, 190120, 190121, 190122, 190123, 190124, 190125, 190118, 190115, 105035, 105029, 105113, 105048, 86539, 86538, 4869, 4868, 4867, 4866, 192075, 192074, 192073, 192072, 192071, 192070, 192362, 192361, 192360, 192359, 192358, 192357, 192356, 192355, 192354, 192364, 192363, 194128, 194127, 194126, 194125, 242010, 194124, 1256, 1255, 1254, 140806, 183953, 183954, 183955, 183956, 183957, 183958, 168487, 168488, 10946, 190906, 190641, 196163, 195926, 195927, 195891, 227250, 190640, 168486, 195928, 195929, 195930, 194133, 195975, 194134, 194135, 159524, 159525, 159526, 159527, 195920, 196180, 196181, 195919, 144822, 144823, 195932, 147643, 196155, 196156, 196157, 196158, 196165, 196166, 196167, 196277, 47750, 218982, 88954, 88955, 88999, 89000, 88996, 267540, 88997, 88998, 88956, 88957, 88958, 88959, 88960, 88961, 88962, 88963, 88964, 88965, 88966, 2186, 190661, 55915, 88968, 88969, 88970, 88971, 88972, 147637, 147638, 147639, 147640, 157785, 157786, 157787, 157788, 157789, 157790, 157791, 157792, 157771, 157772, 157773, 157774, 157775, 157776, 157777, 157778, 157779, 157799, 88399, 88400, 88401, 88402, 88403, 88404, 31711, 31712, 88130, 88131, 88577, 88578, 218566, 218567, 150423, 217842, 217843, 217844, 217845, 217846, 217847, 217848, 217849, 217850, 217840, 217841, 150421, 56254, 56253, 56256, 56257, 56257, 56256, 56250, 56252, 150421, 150422, 56237, 56238, 56239, 56240, 159573, 159574, 159575, 159576, 159577, 159578, 31714, 72085, 72086, 72084, 222693, 180103, 180102, 180109, 180110, 180111, 180112, 72063, 180108, 180107, 180106, 180105, 72004, 72005, 72000, 71999, 161753, 161752, 161755, 161756, 161757, 161758, 161754, 17204, 17201, 17202, 17207, 17208, 88417, 88416, 88418, 36049, 36048, 157952, 157951, 157953, 60348, 60347, 60346, 60345, 60333, 60322, 60323, 60324, 260718, 260719, 260716, 260717, 167852, 60332, 60328, 3765, 3764, 3763, 3762, 3761, 3760, 3759, 3758, 3757, 3756, 3755, 3754, 3753, 3752, 3751, 3750, 3749, 3748, 3747, 3746, 3745, 255366, 255364, 255363, 255362, 255361, 255360, 255359, 255358, 255357, 255356, 255355, 3734, 277860, 116401, 116402, 185716, 225878, 72297, 72298, 72299, 183365, 183366, 255243, 255245, 108927, 108928, 108929, 183367, 183368, 183369, 72295, 277859, 72309, 72310, 153606, 221947, 221948, 221949, 13963, 157940, 144800, 154004, 72288, 72286, 277844, 144801, 144802, 72289, 72291, 151254, 72294, 168525, 168523, 168524, 72293, 277858, 151250, 151251, 221398, 59049, 59050, 59047, 59048, 59046, 277857, 277856, 28905, 28906, 183851, 183850, 183853, 183854, 108921, 183852, 183849, 183848, 183847, 183846, 108923, 108924, 108925, 186147, 186148, 186149, 6512, 222820, 26709, 19073, 19074, 127727, 127728, 127729, 127735, 127734, 127732, 127731, 127744, 127745, 127746, 277855, 222821, 222822, 222823, 222824, 127737, 167355, 167356, 127730, 101208, 167357, 101193, 184448, 101134, 251610, 231224, 231223, 101116, 159748, 159747, 101176, 76871, 254056, 76874, 76875, 231165, 231166, 231167, 158066, 61533, 61457, 61456, 231184, 231185, 76869, 76870, 179755, 179756, 127696, 277854, 127699, 127694, 127695, 156056, 156081, 156061, 101211, 101194, 101181, 101182, 156087, 156088, 156089, 156091, 101215, 231217, 156086, 76740, 76737, 76734, 76742, 76743, 76744, 134870, 127687, 127679, 180033, 127686, 127681, 127684, 127685, 165980, 36194, 147607, 147608, 26714, 26715, 26716, 26717, 189464, 189466, 218168, 115956, 251380, 115950, 168675, 168674, 36197, 36196, 36195, 184637, 241738, 241737, 241739, 76827, 76826, 115935, 115937, 115938, 115938, 115937, 115935, 76826, 76827, 241739, 241737, 241738, 184637, 36195, 36196, 36197, 168674, 168675, 115948, 115949, 115945, 115955, 251379, 115957, 115958, 115959, 189465, 225783, 189464, 26717, 26716, 26715, 26714, 147608, 147607, 36194, 165980, 127690, 221953, 250520, 250518, 250517, 250513, 250514, 250511, 250509, 250506, 133696, 250502, 250503, 19075, 19076, 250500, 250498, 125956, 63943, 125616, 110971, 159753, 110979, 110980, 124303, 223875, 223501, 223500, 127486, 159752, 124308, 124309, 110972, 125955, 250495, 250494, 125610, 1550, 125608, 125609, 127082, 127085, 127086, 127079, 127080, 127090, 53210, 53209, 53208, 53207, 53206, 53205, 246202, 246210, 246209, 246208, 246208, 246211, 53207, 53208, 53209, 53210, 246172, 246171, 246161, 246162, 246163, 246164, 246165, 246166, 246167, 204756, 204763, 204764, 204765, 204761, 204762, 147802, 127123, 127124, 127125, 191423, 231024, 190512, 190513, 95404, 231025, 231026, 231027, 95405, 95406, 95407, 128595, 128597, 128598, 231029, 231030, 231031, 231028, 128600, 246117, 203132, 246116, 254061, 246121, 203100, 203099, 203098, 203098, 203099, 203100, 246122, 246117, 203132, 203133, 246118, 203131, 147803, 246126, 231035, 231036, 246127, 231037, 231038, 230975, 230976, 246134, 230977, 246135, 246136, 230978, 230979, 230980, 1549, 266826, 266821, 134165, 121640, 16204, 16205, 122699, 122696, 141802, 122701, 122700, 250553, 141804, 122695, 122698, 122697, 134, 135, 116745, 250649, 12, 46986, 89320, 138, 139, 250648, 122564, 98651, 250647, 430, 136127, 136132, 103595, 136170, 134563, 166009, 136165, 136166, 149869, 136164, 57122, 57126, 57127, 57128, 57129, 57130, 209364, 209364, 57130, 57129, 57128, 57127, 57126, 57122, 51359, 51360, 183505, 136160, 166008, 171200, 57123, 23, 98652, 122565, 7};


    if(draw_path){
    g->set_color(ezgl::ORANGE);
    g->set_line_width(4);
    for (int i = 0; i <= draw_index; i++) {
        StreetSegmentIdx segment = to_draw[i];
        std::vector<std::pair<ezgl::point2d, ezgl::point2d>> points = globals.all_street_segments[segment].lines_to_draw;
        for (auto pair: points) {
            g->draw_line(pair.first, pair.second);
        }

    }
}

    for(auto depot :depots){
        g->set_color(ezgl::RED);
        ezgl::point2d incre(700,700);
        g->fill_rectangle(globals.all_intersections[depot].position-incre ,globals.all_intersections[depot].position + incre);
    }
    for (int i = 0; i<deliveries.size(); i++) {
        DeliveryInf current = deliveries[i];
        g->set_color(ezgl::DARK_GREEN);
        ezgl::point2d incre(700,700);

        g->fill_rectangle(globals.all_intersections[current.pickUp].position-incre,globals.all_intersections[current.pickUp].position +incre );
        g->set_color(ezgl::BLUE);
        g->fill_rectangle(globals.all_intersections[current.dropOff].position-incre,globals.all_intersections[current.dropOff].position +incre );
        //g->draw_text(globals.all_intersections[current.dropOff].position,name);
    }
    for (int i = 0; i<deliveries.size(); i++) {
        DeliveryInf current = deliveries[i];
        g->set_color(ezgl::BLACK);
        std::string name(1, 'a' + i);
        g->set_font_size(15);
        g->draw_text(globals.all_intersections[current.pickUp].position,name);
        g->set_color(ezgl::WHITE);
        //g->fill_rectangle(globals.all_intersections[current.dropOff].position -incre,globals.all_intersections[current.pickUp].position +incre );
        g->draw_text(globals.all_intersections[current.dropOff].position,name);
    }

}

void drawHighlightedIntersections(ezgl::renderer* g){
    for (int i = 0; i < getNumIntersections(); ++i) {

        intersection_info info = globals.all_intersections[i];

        if (info.highlight){
            g->draw_surface(globals.vec_png.zoom_out[POI_category::HIGHLIGHT], {info.position.x, info.position.y}, 0.025);
        }
    }
}


void drawStreets(ezgl::renderer* g) {
    g->set_line_width(1);
    g->set_line_cap(ezgl::line_cap::butt); // butt ends
    g->set_line_dash(ezgl::line_dash::none); // solid line

    ezgl::rectangle current_zoom_rectangle = g->get_visible_world();
    double x_zoom = current_zoom_rectangle.m_first.x;
    double y_zoom = current_zoom_rectangle.m_second.y;

    bool draw = true;

    for (auto i: globals.all_street_segments) {
        int line_width = -1;
        // check the LOD for the current street segment
        for (uint j = 0; j < i.zoom_levels.size(); ++j) {

            if (current_zoom_level > i.zoom_levels[j].first) {
                line_width = i.zoom_levels[j].second;
                draw = true;
            }

        }
        if (line_width == -1) {
            draw = false;
        }

        if (draw && (i.x_avg > x_zoom && i.y_avg < y_zoom)) {

            // drawing streets
            g->set_line_width(line_width);
            if (!globals.dark_mode){
                g->set_color(i.road_colour);
            }
            else{
                g->set_color(i.dark_road_colour);
            }

            for (uint j = 0; j < i.lines_to_draw.size(); ++j) {
                g->draw_line(i.lines_to_draw[j].first, i.lines_to_draw[j].second);
            }

            // drawing arrows
            if (current_zoom_level >= i.arrow_zoom_dep) {
                g->set_line_width(i.arrow_width);
                g->set_color(i.arrow_colour);
                for (uint j = 0; j < i.arrows_to_draw.size(); ++j) {
                    g->draw_line(i.arrows_to_draw[j].first, i.arrows_to_draw[j].second);
                }
            }

            // drawing texts
            g->set_text_rotation(i.text_rotation);
            g->format_font("", ezgl::font_slant::normal, ezgl::font_weight::bold, 12);
            if (!globals.dark_mode){
                g->set_color(i.text_colour);
            }
            else{
                g->set_color(i.dark_text_colour);
            }
            for (uint j = 0; j < i.text_to_draw.size(); ++j) {
                g->draw_text(i.text_to_draw[j].loc, i.text_to_draw[j].label, i.text_to_draw[j].length_x, i.text_to_draw[j].length_y);
            }
        }
    }
}



void draw_features(ezgl::renderer *g) {
    int count = 0;
    int count2 = 0;
    ezgl::rectangle current_zoom_rectangle = g->get_visible_world();
    double x_zoom_1 = current_zoom_rectangle.m_first.x;
    double y_zoom_1 = current_zoom_rectangle.m_second.y;
    double x_zoom_2 = current_zoom_rectangle.m_second.x;
    double y_zoom_2 = current_zoom_rectangle.m_first.y;

    std::vector<feature_info> queue;
    for (auto i: closed_features) {
        if (current_zoom_level > i.zoom_lod) {
            if ((x_zoom_2 > i.x_min || x_zoom_1 < i.x_max) && (y_zoom_1 > i.y_min || y_zoom_2 < i.y_max)) {
                if (!globals.dark_mode) {
                    g->set_color(i.mycolour);
                }
                else {
                    g->set_color(i.dark_colour);
                }
                if (i.points.size() > 1) {
                    g->fill_poly(i.points);
                }
                count++;
            }
            else {
                count2++;
            }
        }
    }
}


void way_draw_features(ezgl::renderer *g) {
    g->set_line_width(1);
    for (auto i : m2_local_all_ways_info) {

        if (i.way_use != way_enums::notrail && i.way_points2d.size() > 1 && current_zoom_level > 4) { // non-closed way
            for (int j = 0; j < i.way_points2d.size() - 1; ++j) {
                g->set_color(ezgl::GREY_75);
                g->draw_line(i.way_points2d[j], i.way_points2d[j+1]);
            }
        }
    }
}


void drawPOIPng(ezgl::renderer *g) {
    double png_scale_zoomin = 0.004 * current_zoom_level;
    double png_scale = 0.006 * current_zoom_level;
    double text_scale = 1.5 * current_zoom_level;
    double text_scale_zoomin = 0.6 * current_zoom_level;
    bool zoomed_out = (current_zoom_level >= 7) ? false : true;
    double num_scale = 0.058 * current_zoom_level;
    //culling implementation

    ezgl::rectangle current_zoom_rectangle = g->get_visible_world();
    double x_max = current_zoom_rectangle.top_right().x;
    double x_min = current_zoom_rectangle.bottom_left().x;
    double y_max = current_zoom_rectangle.top_right().y;
    double y_min = current_zoom_rectangle.bottom_left().y;


    if (current_zoom_level >= 17) {
        num_scale = 1;
    }

    // always draw these png
    if (current_zoom_level > 3 && globals.draw_which_poi[entertainment]) {
        double scale = png_scale * 1.7;
        if (current_zoom_level > 6) {
            scale = png_scale_zoomin * 1.3;
        }
        for (uint i = 0; i < globals.city_restaurants.size(); ++i) {
            g->draw_surface(globals.vec_png.zoom_in[PIN], globals.city_restaurants[i].poi_loc, scale);
        }
        for (uint i = 0; i < globals.city_shops.size(); ++i) {
            g->draw_surface(globals.vec_png.zoom_out[GROCERY], globals.city_shops[i].poi_loc, 0.7 * scale);
        }
    }
    if (current_zoom_level > 0) {
        if (globals.draw_which_poi[station]) {
            if (!globals.draw_which_poi[NUM_POI_class + 1]) {
                drawSubwayLines(g);
            }
        }
    }

    if (zoomed_out) {
        if (current_zoom_level > 4) {
            //draw some basic, subway and entertainment poi
            if (globals.draw_which_poi[basic]) {
                drawPOIType(g, globals.poi_sorted.basic_poi[hospital], HOSPITAL, png_scale, x_max, x_min, y_max, y_min);
                drawPOIType(g, globals.poi_sorted.basic_poi[emergency_room], HOSPITAL, png_scale, x_max, x_min, y_max, y_min);
                //drawPOIType(g,globals.poi_sorted.basic_poi[pharmacy],HOSPITAL,0.1*current_zoom_level,current_zoom_level);
            }
            drawPOIType(g, globals.poi_sorted.stations_poi, SUBWAY, 0.002 * current_zoom_level, x_max, x_min, y_max, y_min);
            if (globals.draw_which_poi[entertainment]) {
                for (int type = 0; type < NUM_POI_entertainment; type++) {
                    std::vector<POI_info> inner_vector = globals.poi_sorted.entertainment_poi[type];
                    for (int inner = 0; inner < (inner_vector.size() * 0.0008 * current_zoom_level); inner++) {
                        ezgl::point2d anchor = inner_vector[inner].poi_loc;
                        g->draw_surface(globals.vec_png.zoom_out[inner_vector[inner].poi_category], anchor, png_scale);
                    }
                }
            }
        }
    }
    else {
        //zoom in levels
        ezgl::point2d increment(0, 0);

        if (globals.draw_which_poi[basic]) {
            drawPOIName(g, basic, text_scale_zoomin, num_scale, increment, x_max, x_min, y_max, y_min);
        }


        if (current_zoom_level > 13) {
            if (globals.draw_which_poi[subordinate]) {
                drawPOIName(g, subordinate, text_scale_zoomin, num_scale, increment, x_max, x_min, y_max, y_min);
            }
            if (globals.draw_which_poi[neglegible]) {
                drawPOIName(g, neglegible, text_scale_zoomin, num_scale, increment, x_max, x_min, y_max, y_min);
            }
        }

        if (current_zoom_level > 11 && globals.draw_which_poi[subordinate]) {
            for (int type = 0; type < NUM_POI_subordinate; type++) {
                std::vector<POI_info> inner_vector = globals.poi_sorted.subordinate_poi[type];
                for (int inner = 0; inner < inner_vector.size() * num_scale; inner++) {
                    ezgl::point2d anchor = inner_vector[inner].poi_loc;
                    if (anchor.x < x_max && anchor.x > x_min && anchor.y < y_max && anchor.y > y_min) {
                        g->draw_surface(globals.vec_png.zoom_in[SUBORDINATE], anchor, png_scale_zoomin);
                    }
                }
            }
        }

        if (globals.draw_which_poi[entertainment]) {
            if ((current_zoom_level > 10) || (!globals.draw_which_poi[basic])) {
                if (!globals.draw_which_poi[basic] && current_zoom_level < 10) {
                    num_scale = 0.2 * num_scale;
                }
                for (int type = 0; type < NUM_POI_entertainment; type++) {
                    std::vector<POI_info> inner_vector = globals.poi_sorted.entertainment_poi[type];
                    for (int inner = 0; inner < (inner_vector.size() * num_scale); inner++) {
                        ezgl::point2d anchor = inner_vector[inner].poi_loc;
                        if (anchor.x < x_max && anchor.x > x_min && anchor.y < y_max && anchor.y > y_min) {
                            if (type <= Food_Bev_end) {
                                g->draw_surface(globals.vec_png.zoom_in[FOOD], anchor, png_scale_zoomin);
                            }
                            else if (type <= Ent_end) {
                                g->draw_surface(globals.vec_png.zoom_in[ENTERTAINMENT], anchor, png_scale_zoomin);
                            }
                        }
                    }
                }
            }
            if (current_zoom_level > 11) {
                drawPOIName(g, entertainment, text_scale_zoomin, num_scale, increment, x_max, x_min, y_max, y_min);
            }
        }

        if (current_zoom_level > 6) {
            if (globals.draw_which_poi[basic]) {
                for (int type = 0; type < NUM_POI_basics; type++) {
                    std::vector<POI_info> inner_vector = globals.poi_sorted.basic_poi[type];
                    for (int inner = 0; inner < (inner_vector.size() * num_scale); inner++) {
                        ezgl::point2d anchor = inner_vector[inner].poi_loc;
                        if (anchor.x < x_max && anchor.x > x_min && anchor.y < y_max && anchor.y > y_min) {
                            g->draw_surface(globals.vec_png.zoom_in[BASIC], anchor, png_scale_zoomin);
                        }
                    }
                }
            }
            if (globals.draw_which_poi[station]) {
                drawPOIType(g, globals.poi_sorted.stations_poi, SUBWAY, 0.002 * current_zoom_level, x_max, x_min, y_max, y_min);
            }
        }

        if (current_zoom_level >= 7 && globals.draw_which_poi[station]) {
            if (current_zoom_level < 10) {
                ezgl::point2d incre(0, 50);
                drawPOIName(g, station, 1.3 * text_scale, num_scale, incre, x_max, x_min, y_max, y_min);
            }
            else if (current_zoom_level < 13) {
                ezgl::point2d incre(0, 20);
                drawPOIName(g, station, 1.3 * text_scale, num_scale, incre, x_max, x_min, y_max, y_min);
            }
            else {
                drawPOIName(g, station, 1.3 * text_scale, num_scale, increment, x_max, x_min, y_max, y_min);
            }
        }

    }
}

void loadNewMap(const std::string& new_city,ezgl::application* application) {
    std::string new_map_path;

    for( const auto& outer : globals.map_names) {
        const std::string country = outer.first;
        const auto& cities = outer.second;
        for(const auto& city : cities) {
            if(city.first == new_city) {
                new_map_path = city.second;
            }
        }
    }

    closeMap();
    loadMap(new_map_path);
    double max_y = lat_to_y(globals.max_lat);
    double min_y = lat_to_y(globals.min_lat);
    double max_x = lon_to_x(globals.max_lon);
    double min_x = lon_to_x(globals.min_lon);
    ezgl::point2d max_coord(max_x, max_y);
    ezgl::point2d min_coord(min_x, min_y);
    ezgl::rectangle new_coord(min_coord, max_coord);
    application->change_canvas_world_coordinates("MainCanvas", new_coord);
    application->refresh_drawing();
}