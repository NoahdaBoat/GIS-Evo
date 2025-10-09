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

#include <gtk/gtk.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include "gtk4_types.hpp"
#include "ms1helpers.h"
#include "ms2helpers.hpp"
#include "ms3helpers.hpp"
#include "globals.h"
#include "Coordinates_Converstions/coords_conversions.hpp"
#include "OSMEntity_Helpers/m2_way_helpers.hpp"
#include "OSMEntity_Helpers/typed_osmid_helper.hpp"
#include "Intersections/intersection_setup.hpp"
#include "sort_streetseg/streetsegment_info.hpp"
#include "m3_algo/astaralgo.hpp"
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
#include <numbers>
#include <limits>

#define VISUALIZE

// Set up the ezgl graphics window and hand control to it, as shown in the
// ezgl example program.
// This function will be called by both the unit tests (ece297exercise)
// and your main() function in main/src/main.cpp.
// The unit tests always call loadMap() before calling this function
// and call closeMap() after this function returns.


// Global view state for pan/zoom
struct ViewState {
    double offset_x = 0.0;
    double offset_y = 0.0;
    double zoom = 1.0;
    int canvas_width = 0;
    int canvas_height = 0;
    // Aliases for compatibility with existing code
    double center_x = 0.0;
    double center_y = 0.0;
    int width = 0;
    int height = 0;
    Rectangle visible_world{0, 0, 0, 0};
    GtkWidget *drawing_area = nullptr;  // For triggering redraws
};

ViewState g_view_state;

// Helper functions for coordinate transformations
Point2D screen_to_world(Point2D screen) {
    double world_x = (screen.x - g_view_state.canvas_width/2.0 - g_view_state.offset_x) / g_view_state.zoom;
    double world_y = (screen.y - g_view_state.canvas_height/2.0 - g_view_state.offset_y) / g_view_state.zoom;
    return Point2D{world_x, world_y};
}

Point2D world_to_screen(Point2D world) {
    double screen_x = world.x * g_view_state.zoom + g_view_state.canvas_width/2.0 + g_view_state.offset_x;
    double screen_y = world.y * g_view_state.zoom + g_view_state.canvas_height/2.0 + g_view_state.offset_y;
    return Point2D{screen_x, screen_y};
}

void calculate_visible_world() {
    // Calculate world coordinates visible in the current view
    Point2D top_left = screen_to_world(Point2D{0, 0});
    Point2D bottom_right = screen_to_world(Point2D{
        static_cast<double>(g_view_state.canvas_width),
        static_cast<double>(g_view_state.canvas_height)
    });
    
    g_view_state.visible_world = Rectangle{
        top_left.x,
        top_left.y,
        bottom_right.x,
        bottom_right.y
    };

    g_view_state.width = g_view_state.visible_world.width();
    g_view_state.height = g_view_state.visible_world.height();
    g_view_state.center_x = g_view_state.visible_world.center_x();
    g_view_state.center_y = g_view_state.visible_world.center_y();
}

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

constexpr IntersectionIdx INVALID_INTERSECTION = std::numeric_limits<IntersectionIdx>::max();

std::pair<IntersectionIdx, Point2D> clicked_intersection{INVALID_INTERSECTION, Point2D{}};
std::pair<IntersectionIdx, Point2D> origin_intersection{INVALID_INTERSECTION, Point2D{}};
std::pair<IntersectionIdx, Point2D> destination_intersection{INVALID_INTERSECTION, Point2D{}};
std::unordered_set<IntersectionIdx> highlighted_intersections;
std::vector<StreetSegmentIdx> highlighted_route;
int draw_index = 0;
bool draw_path = false;
int current_zoom_level = 0;
double x_zoom_prev, y_zoom_prev;
bool valid_input = false;
StreetSegmentIdx street_to_highlight = -1;
GtkApplication* global_access;
bool search_route = false;
bool set_origin = true;


void clearAllHighlights(GtkApplication* application) {

    for (int i = 0; i < getNumIntersections(); ++i) {
        intersection_info& info = globals.all_intersections[i];
        info.highlight = false;
    }
    
    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
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


void searchEntryEnter(GtkEntry* search_bar, GtkApplication* application) {

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
        
        if (g_view_state.drawing_area) {
            gtk_widget_queue_draw(g_view_state.drawing_area);
        }
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

        if (g_view_state.drawing_area) {
            gtk_widget_queue_draw(g_view_state.drawing_area);
        }

        application->create_popup_message("Intersection(s) Information", message.c_str());
    }
}

void searchEntryType(GtkEntry* search_bar, GtkApplication* application) {

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

void zoomFit(GtkEntry* /*zoom_fit_button*/, GtkApplication* application) {
    current_zoom_level = 1;
    
    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
}

void change_map(GtkEntry* city_maps, GtkApplication* application) {
    GtkComboBoxText* list_cities = GTK_COMBO_BOX_TEXT(city_maps);
    gchar* selected_city = gtk_combo_box_text_get_active_text(list_cities);
    std::string new_city = selected_city;
    std::cout<<"Changing to: " << new_city << std::endl;
    loadNewMap(new_city, application);
}

void draw_ent(GtkEntry* /*ent_buttom*/, GtkApplication* application) {
    if(globals.draw_which_poi[NUM_POI_class+1]) {
        setAllBool(false);
    }
        globals.draw_which_poi[entertainment] = !globals.draw_which_poi[entertainment] ;
    
    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
}

void draw_trans(GtkEntry* /*trans_buttom*/, GtkApplication* application) {
    if(globals.draw_which_poi[NUM_POI_class+1]) {
        setAllBool(false);
    }
    globals.draw_which_poi[station] = !globals.draw_which_poi[station] ;
    
    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
}

void draw_basic(GtkEntry* /*basic_buttom*/, GtkApplication* application) {
    if(globals.draw_which_poi[NUM_POI_class+1]) {
        setAllBool(false);
    }
    globals.draw_which_poi[basic] = !globals.draw_which_poi[basic] ;
    
    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
}

void darkMode(GtkEntry* /*dark_mode_button*/, GtkApplication* application) {
   globals.dark_mode = !globals.dark_mode;
   
   if (g_view_state.drawing_area) {
       gtk_widget_queue_draw(g_view_state.drawing_area);
   }
}

void aboutButton(GtkWidget* /*About menu button*/, GtkApplication* application) {
    std::string message;

    message += std::string("Created by: ") + "\n";
    message += std::string("Lanlin He, Nicole Jiao, and Noah Monti") + "\n";
    message += std::string("At the University of Toronto") + "\n";
    message += std::string("-- 2024 --");

    application->create_popup_message("About", message.c_str());
}

void helpButton(GtkWidget* /*Help button */, GtkApplication* application) {
    std::string message;

    message += std::string("Information about this application: ") + "\n" + "\n";
    message += std::string("To search for an intersection, or find a route: Enter the starting intersection in the 'Origin' box, and enter the destination intersection in the 'Destination' box") + "\n" + "\n";
    message += std::string("Points of interest are represented across the map, clicking on one will display its information") + "\n" + "\n";

    application->create_popup_message("Help", message.c_str());

}

void outputRoad(GtkApplication* application) {
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
    
    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
}

void dialogInput(GtkWidget* dialog ,GtkApplication* /*application*/, gpointer input){
    gint response = GPOINTER_TO_INT(input);
    if(response == GTK_RESPONSE_ACCEPT){
        gtk_widget_destroy(dialog);
        clearRoadArrows(highlighted_route);
        highlighted_route.clear();
   }
}

void searchRouteToggle(GtkToggleButton* search_route_toggle, GtkApplication* application){
    search_route = gtk_toggle_button_get_active(search_route_toggle);
    clearAllHighlights(application);
}


void initial_setup(GtkApplication *application, bool /*new_window*/) {
    // Note: This function is for legacy EZGL compatibility
    // The new GTK4 application uses on_activate() instead
    
    #ifdef VISUALIZE
    global_access = application;
    #endif

    // setting our starting row for insertion at 6 (default buttons take up first five rows)
    // increment row each time we insert a new element.
    int row = 6;
    application->create_popup_message("Complete", "All Items Drawn");
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



void actOnMouseClick(GtkApplication* application, GdkEventButton* event, double x, double y) {

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
        message2 += globals.city_shops[index2].city + ", " + globals.city_shops[index2].country + "\n";
        message2 += globals.city_shops[index2].inner_category + "\n";
        message2 += "Rating: " + std::to_string((static_cast<int>(globals.city_shops[index2].rating*10)/10)) + "/10\n";
        message2 += globals.city_shops[index2].website + "\n";
        message2 += "Copyright 2024 Foursquare";
        message2 += "\n";
        application->create_popup_message(title, message2.c_str());
    }

    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
}


// Forward declarations for callbacks
static void on_activate(GtkApplication *app, gpointer user_data);
static void draw_callback(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
static void drag_begin_callback(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data);
static void drag_update_callback(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data);
static gboolean scroll_callback(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data);
static gboolean key_press_callback(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
static void mouse_click_callback(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);

// Global UI widgets for Part D
static GtkWidget *g_status_label = nullptr;
static GtkWidget *g_search_entry = nullptr;


// TASK-089: Update status bar with message
void updateStatusMessage(const std::string& message) {
    if (g_status_label) {
        gtk_label_set_text(GTK_LABEL(g_status_label), message.c_str());
    }
    std::cout << "Status: " << message << std::endl;
}

// TASK-085: Mouse click callback for GTK4 GestureClick
// Handles intersection selection, POI clicks, and route planning
static void mouse_click_callback(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    // Get button that was pressed
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    
    // Right click - context menu (future enhancement)
    if (button == GDK_BUTTON_SECONDARY) {
        updateStatusMessage("Right click - context menu (not yet implemented)");
        return;
    }
    
    // Left click - intersection/POI selection
    if (button == GDK_BUTTON_PRIMARY) {
        // Convert screen coordinates to world coordinates
        double world_x = (x - g_view_state.width / 2.0) / g_view_state.zoom + g_view_state.center_x;
        double world_y = (g_view_state.height / 2.0 - y) / g_view_state.zoom + g_view_state.center_y;
        
        // Clear previous highlights
        highlighted_intersections.clear();
        
        // Find closest intersection using world coordinates
        // Note: This requires conversion functions that may not be available yet
        // For now, we'll use a simplified approach
        
        // Simple distance-based search (placeholder for proper implementation)
        IntersectionIdx closest_intersection = -1;
        double min_distance = std::numeric_limits<double>::max();
        
        // Search through intersections to find closest
        for (size_t i = 0; i < globals.all_intersections.size(); i++) {
            const auto& intersection = globals.all_intersections[i];
            double dx = intersection.position.x - world_x;
            double dy = intersection.position.y - world_y;
            double dist = std::sqrt(dx * dx + dy * dy);
            
            if (dist < min_distance) {
                min_distance = dist;
                closest_intersection = i;
            }
        }
        
        // Check if click is close enough to an intersection (within 150 world units)
        if (min_distance < 150.0 / g_view_state.zoom) {
            // Highlight the intersection
            highlighted_intersections.insert(closest_intersection);
            clicked_intersection = {closest_intersection, globals.all_intersections[closest_intersection].position};
            
            // Update status message
            std::string intersection_name = globals.all_intersections[closest_intersection].name;
            if (intersection_name.find("<unknown>") == std::string::npos) {
                updateStatusMessage("Selected: " + intersection_name);
                
                // Show detailed info in console
                std::cout << "Intersection " << closest_intersection << ": " << intersection_name << std::endl;
            }
            
            // Handle route planning mode
            if (search_route) {
                if (set_origin) {
                    origin_intersection = {closest_intersection, globals.all_intersections[closest_intersection].position};
                    updateStatusMessage("Origin set. Click destination intersection.");
                    set_origin = false;
                } else {
                    destination_intersection = {closest_intersection, globals.all_intersections[closest_intersection].position};
                    updateStatusMessage("Destination set. Finding route...");
                    
                    // Route finding would go here (requires m3 functions)
                    // highlighted_route = findPathBetweenIntersections(...);
                    
                    set_origin = true;
                    highlighted_intersections.insert(origin_intersection.first);
                }
            }
        } else {
            updateStatusMessage("No intersection nearby");
        }
        
        // Trigger redraw
        if (g_view_state.drawing_area) {
            gtk_widget_queue_draw(g_view_state.drawing_area);
        }
    }
}

// TASK-087: Search entry callback (simplified version)
static void search_entry_activate_callback(GtkEntry *entry, gpointer user_data) {
    const char *search_text = gtk_entry_get_text(entry);
    std::string search_str(search_text);
    
    if (search_str.empty()) {
        updateStatusMessage("Enter a search term");
        return;
    }
    
    updateStatusMessage("Searching for: " + search_str);
    
    // Search intersections by name (simplified - requires m1 functions)
    // This is a placeholder for actual search implementation
    bool found = false;
    for (size_t i = 0; i < globals.all_intersections.size(); i++) {
        const auto& intersection = globals.all_intersections[i];
        if (intersection.name.find(search_str) != std::string::npos) {
            // Found a match
            highlighted_intersections.clear();
            highlighted_intersections.insert(i);
            clicked_intersection = {static_cast<int>(i), intersection.position};
            
            // Center map on this intersection
            g_view_state.center_x = intersection.position.x;
            g_view_state.center_y = intersection.position.y;
            
            updateStatusMessage("Found: " + intersection.name);
            found = true;
            
            // Redraw
            if (g_view_state.drawing_area) {
                gtk_widget_queue_draw(g_view_state.drawing_area);
            }
            break;
        }
    }
    
    if (!found) {
        updateStatusMessage("No intersection found matching: " + search_str);
    }
}

// TASK-088: Button callback for route search toggle
static void route_search_button_callback(GtkButton *button, gpointer user_data) {
    search_route = !search_route;
    
    if (search_route) {
        gtk_button_set_label(button, "Cancel Route Search");
        set_origin = true;
        updateStatusMessage("Route search mode: Click origin intersection");
    } else {
        gtk_button_set_label(button, "Find Route");
        highlighted_route.clear();
        origin_intersection = {-1, Point2D{0, 0}};
        destination_intersection = {-1, Point2D{0, 0}};
        updateStatusMessage("Route search cancelled");
        
        if (g_view_state.drawing_area) {
            gtk_widget_queue_draw(g_view_state.drawing_area);
        }
    }
}

// Main function, called from main.cpp
void drawMap() {
    // Create GTK4 application
    GtkApplication *app = gtk_application_new("com.noahboat.gisevo.mapper", G_APPLICATION_FLAGS_NONE);
    
    // Connect activation callback
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    
    // Run application event loop (this blocks until window closes)
    int status = g_application_run(G_APPLICATION(app), 0, nullptr);
    
    // Cleanup
    g_object_unref(app);
}

// Application activation callback - creates main window
static void on_activate(GtkApplication *app, gpointer user_data) {
    // Create application window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "GIS Evo - Map Navigator");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 900);
    
    // Create main vertical box container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(vbox, 5);
    gtk_widget_set_margin_end(vbox, 5);
    gtk_widget_set_margin_top(vbox, 5);
    gtk_widget_set_margin_bottom(vbox, 5);
    
    // TASK-087 & TASK-088: Create toolbar with search and buttons
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(toolbar, 5);
    gtk_widget_set_margin_end(toolbar, 5);
    
    // Search entry
    g_search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_search_entry), "Search intersections...");
    gtk_widget_set_hexpand(g_search_entry, TRUE);
    g_signal_connect(g_search_entry, "activate", G_CALLBACK(search_entry_activate_callback), nullptr);
    gtk_box_append(GTK_BOX(toolbar), g_search_entry);
    
    // Route search button
    GtkWidget *route_button = gtk_button_new_with_label("Find Route");
    g_signal_connect(route_button, "clicked", G_CALLBACK(route_search_button_callback), nullptr);
    gtk_box_append(GTK_BOX(toolbar), route_button);
    
    // Dark mode toggle button
    GtkWidget *dark_mode_button = gtk_button_new_with_label("Toggle Dark Mode");
    g_signal_connect(dark_mode_button, "clicked", G_CALLBACK([](GtkButton *btn, gpointer data) {
        globals.dark_mode = !globals.dark_mode;
        if (g_view_state.drawing_area) {
            gtk_widget_queue_draw(g_view_state.drawing_area);
        }
        updateStatusMessage(globals.dark_mode ? "Dark mode enabled" : "Dark mode disabled");
    }), nullptr);
    gtk_box_append(GTK_BOX(toolbar), dark_mode_button);
    
    // Clear selections button
    GtkWidget *clear_button = gtk_button_new_with_label("Clear");
    g_signal_connect(clear_button, "clicked", G_CALLBACK([](GtkButton *btn, gpointer data) {
        highlighted_intersections.clear();
        highlighted_route.clear();
        clicked_intersection = {-1, Point2D{0, 0}};
        origin_intersection = {-1, Point2D{0, 0}};
        destination_intersection = {-1, Point2D{0, 0}};
        if (g_view_state.drawing_area) {
            gtk_widget_queue_draw(g_view_state.drawing_area);
        }
        updateStatusMessage("Selections cleared");
    }), nullptr);
    gtk_box_append(GTK_BOX(toolbar), clear_button);
    
    gtk_box_append(GTK_BOX(vbox), toolbar);
    
    // Create drawing area for Cairo rendering
    g_view_state.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(g_view_state.drawing_area, TRUE);
    gtk_widget_set_vexpand(g_view_state.drawing_area, TRUE);
    
    // Set up Cairo draw callback
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(g_view_state.drawing_area),
                                    draw_callback, nullptr, nullptr);
    
    // TASK-085: Add mouse click gesture controller
    GtkGesture *click = gtk_gesture_click_new();
    gtk_widget_add_controller(g_view_state.drawing_area, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(click, "pressed", G_CALLBACK(mouse_click_callback), nullptr);
    
    // Add drag gesture controller for panning
    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_widget_add_controller(g_view_state.drawing_area, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin_callback), nullptr);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update_callback), nullptr);
    
    // Add scroll controller for zooming
    GtkEventController *scroll = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    gtk_widget_add_controller(g_view_state.drawing_area, scroll);
    g_signal_connect(scroll, "scroll", G_CALLBACK(scroll_callback), nullptr);
    
    // Add keyboard controller
    GtkEventController *key = gtk_event_controller_key_new();
    gtk_widget_add_controller(g_view_state.drawing_area, key);
    g_signal_connect(key, "key-pressed", G_CALLBACK(key_press_callback), nullptr);
    
    gtk_box_append(GTK_BOX(vbox), g_view_state.drawing_area);
    
    // TASK-089: Create status bar
    g_status_label = gtk_label_new("Ready");
    gtk_widget_set_halign(g_status_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(g_status_label, 10);
    gtk_widget_set_margin_end(g_status_label, 10);
    gtk_widget_set_margin_top(g_status_label, 5);
    gtk_widget_set_margin_bottom(g_status_label, 5);
    
    // Create a separator and status bar container
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(vbox), separator);
    gtk_box_append(GTK_BOX(vbox), g_status_label);
    
    // Set window content and show
    gtk_window_set_child(GTK_WINDOW(window), vbox);
    gtk_widget_grab_focus(g_view_state.drawing_area);
    gtk_window_present(GTK_WINDOW(window));
    
    updateStatusMessage("GIS Evo Map Navigator initialized - Ready to explore!");
    
    std::cout << "GIS Evo Map Navigator initialized successfully!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - Click to select intersections" << std::endl;
    std::cout << "  - Drag to pan" << std::endl;
    std::cout << "  - Scroll to zoom" << std::endl;
    std::cout << "  - Press 'd' to toggle dark mode" << std::endl;
    std::cout << "  - Press '+/-' to zoom in/out" << std::endl;
    std::cout << "  - Press 'c' to clear selections" << std::endl;
    std::cout << "  - Use search bar to find intersections" << std::endl;
    std::cout << "  - Click 'Find Route' to plan a route" << std::endl;
}

// Cairo drawing callback
static void draw_callback(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    // Update canvas dimensions
    g_view_state.canvas_width = width;
    g_view_state.canvas_height = height;
    
    // Delegate to main canvas drawing function
    draw_main_canvas(cr, width, height);
}

// Drag gesture callbacks for panning
static double drag_start_offset_x = 0.0;
static double drag_start_offset_y = 0.0;

static void drag_begin_callback(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data) {
    drag_start_offset_x = g_view_state.offset_x;
    drag_start_offset_y = g_view_state.offset_y;
}

static void drag_update_callback(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data) {
    g_view_state.offset_x = drag_start_offset_x + offset_x;
    g_view_state.offset_y = drag_start_offset_y + offset_y;
    
    // Trigger redraw
    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
}

// Scroll callback for zooming
static gboolean scroll_callback(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data) {
    // Zoom in/out based on scroll direction
    double zoom_factor = 1.1;
    if (dy < 0) {
        // Scroll up - zoom in
        g_view_state.zoom *= zoom_factor;
    } else {
        // Scroll down - zoom out
        g_view_state.zoom /= zoom_factor;
    }
    
    // Clamp zoom to reasonable range
    if (g_view_state.zoom < 0.1) g_view_state.zoom = 0.1;
    if (g_view_state.zoom > 100.0) g_view_state.zoom = 100.0;
    
    // Trigger redraw
    if (g_view_state.drawing_area) {
        gtk_widget_queue_draw(g_view_state.drawing_area);
    }
    
    return TRUE;  // Event handled
}

// Keyboard callback
static gboolean key_press_callback(GtkEventControllerKey *controller, guint keyval, guint keycode, 
                                   GdkModifierType state, gpointer user_data) {
    switch (keyval) {
        case GDK_KEY_d:
        case GDK_KEY_D:
            // Toggle dark mode
            globals.dark_mode = !globals.dark_mode;
            if (g_view_state.drawing_area) {
                gtk_widget_queue_draw(g_view_state.drawing_area);
            }
            updateStatusMessage(globals.dark_mode ? "Dark mode enabled" : "Dark mode disabled");
            return TRUE;
            
        case GDK_KEY_plus:
        case GDK_KEY_equal:
        case GDK_KEY_KP_Add:
            // Zoom in
            g_view_state.zoom *= 1.2;
            if (g_view_state.zoom > 100.0) g_view_state.zoom = 100.0;
            if (g_view_state.drawing_area) {
                gtk_widget_queue_draw(g_view_state.drawing_area);
            }
            updateStatusMessage("Zoomed in");
            return TRUE;
            
        case GDK_KEY_minus:
        case GDK_KEY_underscore:
        case GDK_KEY_KP_Subtract:
            // Zoom out
            g_view_state.zoom /= 1.2;
            if (g_view_state.zoom < 0.1) g_view_state.zoom = 0.1;
            if (g_view_state.drawing_area) {
                gtk_widget_queue_draw(g_view_state.drawing_area);
            }
            updateStatusMessage("Zoomed out");
            return TRUE;
            
        case GDK_KEY_c:
        case GDK_KEY_C:
            // Clear selections
            highlighted_intersections.clear();
            highlighted_route.clear();
            clicked_intersection = {-1, Point2D{0, 0}};
            origin_intersection = {-1, Point2D{0, 0}};
            destination_intersection = {-1, Point2D{0, 0}};
            if (g_view_state.drawing_area) {
                gtk_widget_queue_draw(g_view_state.drawing_area);
            }
            updateStatusMessage("Selections cleared");
            return TRUE;
            
        case GDK_KEY_r:
        case GDK_KEY_R:
            // Toggle route search mode
            search_route = !search_route;
            if (search_route) {
                set_origin = true;
                updateStatusMessage("Route search mode: Click origin intersection");
            } else {
                highlighted_route.clear();
                updateStatusMessage("Route search cancelled");
                if (g_view_state.drawing_area) {
                    gtk_widget_queue_draw(g_view_state.drawing_area);
                }
            }
            return TRUE;
            
        case GDK_KEY_f:
        case GDK_KEY_F:
            // Focus on search entry
            if (g_search_entry) {
                gtk_widget_grab_focus(g_search_entry);
                updateStatusMessage("Search mode - enter intersection name");
            }
            return TRUE;
            
        case GDK_KEY_h:
        case GDK_KEY_H:
        case GDK_KEY_question:
            // Show help message
            updateStatusMessage("Keys: d=dark mode, +/-=zoom, c=clear, r=route, f=search, h=help");
            std::cout << "\n=== GIS Evo Keyboard Shortcuts ===" << std::endl;
            std::cout << "  d/D         - Toggle dark mode" << std::endl;
            std::cout << "  +/=/-       - Zoom in/out" << std::endl;
            std::cout << "  c/C         - Clear all selections" << std::endl;
            std::cout << "  r/R         - Toggle route search mode" << std::endl;
            std::cout << "  f/F         - Focus search bar" << std::endl;
            std::cout << "  h/H/?       - Show this help" << std::endl;
            std::cout << "  Escape      - Cancel/clear" << std::endl;
            std::cout << "================================\n" << std::endl;
            return TRUE;
            
        case GDK_KEY_Escape:
            // Cancel current operation
            if (search_route) {
                search_route = false;
                highlighted_route.clear();
                updateStatusMessage("Cancelled");
                if (g_view_state.drawing_area) {
                    gtk_widget_queue_draw(g_view_state.drawing_area);
                }
            } else {
                highlighted_intersections.clear();
                if (g_view_state.drawing_area) {
                    gtk_widget_queue_draw(g_view_state.drawing_area);
                }
                updateStatusMessage("Cleared");
            }
            return TRUE;
            
        case GDK_KEY_Home:
            // Reset zoom and position
            g_view_state.zoom = 1.0;
            g_view_state.offset_x = 0.0;
            g_view_state.offset_y = 0.0;
            if (g_view_state.drawing_area) {
                gtk_widget_queue_draw(g_view_state.drawing_area);
            }
            updateStatusMessage("View reset to default");
            return TRUE;
    }
    
    return FALSE;  // Event not handled
}



void draw_main_canvas(cairo_t *cr, int width, int height) {
    g_view_state.canvas_width = width;
    g_view_state.canvas_height = height;

    // Save Cairo state
    cairo_save(cr);
    
    // Clear background based on dark mode
    if (globals.dark_mode) {
        cairo_set_source_rgb(cr, 53.0/255.0, 59.0/255.0, 66.0/255.0);  // Dark gray
    } else {
        cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);  // Light gray
    }
    cairo_paint(cr);
    
    // Calculate visible world coordinates
    calculate_visible_world();
    
    // Apply pan and zoom transformations
    cairo_translate(cr, width / 2.0 + g_view_state.offset_x, height / 2.0 + g_view_state.offset_y);
    cairo_scale(cr, g_view_state.zoom, g_view_state.zoom);
    
    // Update current zoom level for feature filtering
    // TODO: Implement proper zoom level calculation
    // get_current_zoom_level(x_zoom_prev, y_zoom_prev, current_zoom_level, g_view_state.visible_world);
    
    // Draw in order (back to front)
    draw_features(cr);              // Draw map features (parks, buildings, water)
    way_draw_features(cr);          // Draw OSM way features
    drawStreets(cr);                // Draw street network
    highlightRoute(cr, highlighted_route);  // Highlight selected route
    redrawStreetComponents(cr, highlighted_route);  // Street names and arrows
    drawHighlightedIntersections(cr);  // Draw selected intersections
    drawPOIPng(cr);                 // Draw points of interest
    
    // For now, draw a simple test pattern
    cairo_set_line_width(cr, 2.0 / g_view_state.zoom);
    
    // Draw coordinate axes for reference
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);  // Red X axis
    cairo_move_to(cr, -10000, 0);
    cairo_line_to(cr, 10000, 0);
    cairo_stroke(cr);
    
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);  // Green Y axis
    cairo_move_to(cr, 0, -10000);
    cairo_line_to(cr, 0, 10000);
    cairo_stroke(cr);
    
    // Draw a test circle at origin
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);  // Blue
    cairo_arc(cr, 0, 0, 100 / g_view_state.zoom, 0, 2 * std::numbers::pi);
    cairo_fill(cr);
    
    // Restore Cairo state
    cairo_restore(cr);
}

// TASK-083: General text rendering helper with PangoLayout
// Reusable function for drawing text with rotation, alignment, and font settings
void drawText(cairo_t* cr, const std::string& text, double world_x, double world_y, 
              const std::string& font_desc_str = "Sans 10", 
              double rotation = 0.0,
              double r = 0.0, double g = 0.0, double b = 0.0) {
    if (!cr || text.empty()) return;
    
    // Transform world coordinates to screen coordinates
    double screen_x = (world_x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
    double screen_y = (g_view_state.center_y - world_y) * g_view_state.zoom + g_view_state.height / 2.0;
    
    // Set text color
    cairo_set_source_rgb(cr, r, g, b);
    
    // Create Pango layout for text rendering
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* font_desc = pango_font_description_from_string(font_desc_str.c_str());
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, text.c_str(), -1);
    
    // Apply transformations
    cairo_save(cr);
    cairo_move_to(cr, screen_x, screen_y);
    
    // Apply rotation if specified
    if (rotation != 0.0) {
        cairo_rotate(cr, rotation);
    }
    
    // Draw the text
    pango_cairo_show_layout(cr, layout);
    
    cairo_restore(cr);
    
    // Cleanup
    pango_font_description_free(font_desc);
    g_object_unref(layout);
}

// TASK-084: Helper function to check if an object should be drawn at current zoom level
// Returns true if the object should be visible at the current zoom level
bool shouldDrawAtZoomLevel(int object_zoom_threshold) {
    // Higher zoom levels mean more detail visible
    // Object is drawn if current_zoom_level >= threshold
    return current_zoom_level >= object_zoom_threshold;
}

// TASK-081: Draw Points of Interest (POIs) based on category and zoom level
// Draws restaurant and shop POIs with simple circle markers and optional text labels
void drawPOIPng(cairo_t* cr) {
    if (!cr) return;
    
    // Only draw POIs at higher zoom levels (zoom > 10 for visibility)
    if (current_zoom_level < 10) return;
    
    // Get visible world bounds for culling
    double visible_bounds[4];
    calculate_visible_world(visible_bounds);
    double x_min = visible_bounds[0];
    double x_max = visible_bounds[1];
    double y_min = visible_bounds[2];
    double y_max = visible_bounds[3];
    
    // POI marker size (constant screen size)
    double marker_radius = 4.0 / g_view_state.zoom;
    
    // Draw restaurants (if category is enabled)
    if (!globals.draw_which_poi.empty() && globals.draw_which_poi[static_cast<int>(POI_category::FOOD)]) {
        cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);  // Red for restaurants
        
        for (const auto& restaurant : globals.city_restaurants) {
            // Convert LatLon to world coordinates (assuming pos is LatLon)
            double world_x = restaurant.pos.longitude();
            double world_y = restaurant.pos.latitude();
            
            // Viewport culling
            if (world_x < x_min || world_x > x_max || world_y < y_min || world_y > y_max) {
                continue;
            }
            
            // Transform to screen coordinates
            double screen_x = (world_x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
            double screen_y = (g_view_state.center_y - world_y) * g_view_state.zoom + g_view_state.height / 2.0;
            
            // Draw circle marker
            cairo_arc(cr, screen_x, screen_y, marker_radius, 0, 2 * M_PI);
            cairo_fill(cr);
            
            // Draw label at very high zoom levels (zoom > 14)
            if (current_zoom_level > 14 && !restaurant.poi_name.empty()) {
                drawText(cr, restaurant.poi_name, world_x, world_y, 
                        "Sans 8", 0.0, 0.0, 0.0, 0.0);  // Black text
            }
        }
    }
    
    // Draw shops (if category is enabled)
    if (!globals.draw_which_poi.empty() && globals.draw_which_poi[static_cast<int>(POI_category::SHOPPING)]) {
        cairo_set_source_rgb(cr, 0.2, 0.2, 1.0);  // Blue for shops
        
        for (const auto& shop : globals.city_shops) {
            // Convert LatLon to world coordinates
            double world_x = shop.pos.longitude();
            double world_y = shop.pos.latitude();
            
            // Viewport culling
            if (world_x < x_min || world_x > x_max || world_y < y_min || world_y > y_max) {
                continue;
            }
            
            // Transform to screen coordinates
            double screen_x = (world_x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
            double screen_y = (g_view_state.center_y - world_y) * g_view_state.zoom + g_view_state.height / 2.0;
            
            // Draw circle marker
            cairo_arc(cr, screen_x, screen_y, marker_radius, 0, 2 * M_PI);
            cairo_fill(cr);
            
            // Draw label at very high zoom levels
            if (current_zoom_level > 14 && !shop.poi_name.empty()) {
                drawText(cr, shop.poi_name, world_x, world_y, 
                        "Sans 8", 0.0, 0.0, 0.0, 0.0);  // Black text
            }
        }
    }
    
    // Draw other POI categories from globals.poi_sorted if needed
    // This can be extended to draw basic_poi, entertainment_poi, etc.
    if (!globals.draw_which_poi.empty() && globals.draw_which_poi[static_cast<int>(POI_category::BASIC)]) {
        cairo_set_source_rgb(cr, 0.0, 0.8, 0.0);  // Green for basic POIs (hospitals, schools, etc.)
        
        for (const auto& poi_vector : globals.poi_sorted.basic_poi) {
            for (const auto& poi : poi_vector) {
                // Get world coordinates from POI_info
                double world_x = poi.poi_loc.x;
                double world_y = poi.poi_loc.y;
                
                // Viewport culling
                if (world_x < x_min || world_x > x_max || world_y < y_min || world_y > y_max) {
                    continue;
                }
                
                // Transform to screen coordinates
                double screen_x = (world_x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
                double screen_y = (g_view_state.center_y - world_y) * g_view_state.zoom + g_view_state.height / 2.0;
                
                // Draw circle marker
                cairo_arc(cr, screen_x, screen_y, marker_radius, 0, 2 * M_PI);
                cairo_fill(cr);
                
                // Draw label at very high zoom levels
                if (current_zoom_level > 14 && !poi.poi_name.empty()) {
                    drawText(cr, poi.poi_name, world_x, world_y, 
                            "Sans 8", 0.0, 0.0, 0.0, 0.0);
                }
            }
        }
    }
}

// TASK-079: Highlight route path between intersections
// Draws thick orange line along route segments
void highlightRoute(cairo_t* cr, const std::vector<StreetSegmentIdx>& route) {
    if (!cr || route.empty()) return;
    
    // Set orange color for route (#FF8C00)
    cairo_set_source_rgb(cr, 1.0, 0.549, 0.0);
    
    // Use thick line width (6 pixels constant screen size)
    cairo_set_line_width(cr, 6.0 / g_view_state.zoom);
    
    // Draw each segment in the route
    for (StreetSegmentIdx segment_idx : route) {
        if (segment_idx < 0 || segment_idx >= globals.all_street_segments.size()) {
            continue;  // Skip invalid segment
        }
        
        const street_segment_info& segment = globals.all_street_segments[segment_idx];
        
        // Draw all line segments for this street segment
        for (const auto& line_pair : segment.lines_to_draw) {
            const Point2D& from = line_pair.first;
            const Point2D& to = line_pair.second;
            
            // Transform to screen coordinates
            double screen_x1 = (from.x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
            double screen_y1 = (g_view_state.center_y - from.y) * g_view_state.zoom + g_view_state.height / 2.0;
            double screen_x2 = (to.x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
            double screen_y2 = (g_view_state.center_y - to.y) * g_view_state.zoom + g_view_state.height / 2.0;
            
            cairo_move_to(cr, screen_x1, screen_y1);
            cairo_line_to(cr, screen_x2, screen_y2);
        }
    }
    
    cairo_stroke(cr);
}

// TASK-080: Redraw street components (names and one-way arrows) for highlighted route
// Draws street names with rotation and one-way arrows for route segments
void redrawStreetComponents(cairo_t* cr, const std::vector<StreetSegmentIdx>& route) {
    if (!cr || route.empty()) return;
    
    // Draw street names and arrows for each segment in route
    for (StreetSegmentIdx segment_idx : route) {
        if (segment_idx < 0 || segment_idx >= globals.all_street_segments.size()) {
            continue;  // Skip invalid segment
        }
        
        const street_segment_info& segment = globals.all_street_segments[segment_idx];
        
        // Draw one-way arrows if this is a one-way street
        if (segment.oneWay && !segment.arrows_to_draw.empty()) {
            // Use bright orange color for arrows on highlighted route
            cairo_set_source_rgb(cr, 1.0, 0.549, 0.0);
            cairo_set_line_width(cr, 3.0 / g_view_state.zoom);
            
            for (const auto& arrow_pair : segment.arrows_to_draw) {
                const Point2D& from = arrow_pair.first;
                const Point2D& to = arrow_pair.second;
                
                // Transform to screen coordinates
                double screen_x1 = (from.x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
                double screen_y1 = (g_view_state.center_y - from.y) * g_view_state.zoom + g_view_state.height / 2.0;
                double screen_x2 = (to.x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
                double screen_y2 = (g_view_state.center_y - to.y) * g_view_state.zoom + g_view_state.height / 2.0;
                
                cairo_move_to(cr, screen_x1, screen_y1);
                cairo_line_to(cr, screen_x2, screen_y2);
                cairo_stroke(cr);
            }
        }
        
        // Draw street names for route segments
        if (!segment.text_to_draw.empty() && !segment.street_name.empty()) {
            // Use bright white text for visibility on highlighted route
            if (globals.dark_mode) {
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);  // White
            } else {
                cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);  // Black
            }
            
            // Create PangoLayout for text rendering
            PangoLayout* layout = pango_cairo_create_layout(cr);
            PangoFontDescription* font_desc = pango_font_description_from_string("Sans Bold 10");
            pango_layout_set_font_description(layout, font_desc);
            
            for (const auto& text_prop : segment.text_to_draw) {
                // Transform text position to screen coordinates
                double screen_x = (text_prop.loc.x - g_view_state.center_x) * g_view_state.zoom + g_view_state.width / 2.0;
                double screen_y = (g_view_state.center_y - text_prop.loc.y) * g_view_state.zoom + g_view_state.height / 2.0;
                
                cairo_save(cr);
                cairo_move_to(cr, screen_x, screen_y);
                
                // Apply rotation based on street angle
                cairo_rotate(cr, segment.text_rotation);
                
                // Set text and draw
                pango_layout_set_text(layout, segment.street_name.c_str(), -1);
                pango_cairo_show_layout(cr, layout);
                
                cairo_restore(cr);
            }
            
            pango_font_description_free(font_desc);
            g_object_unref(layout);
        }
    }
}

void drawHighlightedIntersections(cairo_t* cr){
    // Draw highlighted/selected intersections as circles
    cairo_save(cr);
    
    // Early return if no intersections to highlight
    if (highlighted_intersections.empty()) {
        cairo_restore(cr);
        return;
    }
    
    // Set highlight color (bright yellow/orange for visibility)
    cairo_set_source_rgba(cr, 1.0, 0.65, 0.0, 0.8);  // Orange with some transparency
    
    // Calculate circle radius based on zoom (constant screen size)
    double radius = 8.0 / g_view_state.zoom;  // ~8 pixels on screen
    
    // Draw all highlighted intersections
    for (IntersectionIdx idx : highlighted_intersections) {
        // Check bounds
        if (idx < 0 || idx >= static_cast<int>(globals.all_intersections.size())) {
            continue;
        }
        
        const intersection_info& info = globals.all_intersections[idx];
        
        // Draw filled circle at intersection position
        cairo_arc(cr, info.position.x, info.position.y, radius, 0, 2 * std::numbers::pi);
        cairo_fill(cr);
    }
    
    // Also draw special markers for origin and destination intersections
    // Origin: Green circle
    if (origin_intersection.first >= 0) {
        cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.9);  // Bright green
        cairo_arc(cr, origin_intersection.second.x, origin_intersection.second.y, 
                  radius * 1.2, 0, 2 * std::numbers::pi);
        cairo_fill(cr);
        
        // Add white border
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_set_line_width(cr, 2.0 / g_view_state.zoom);
        cairo_arc(cr, origin_intersection.second.x, origin_intersection.second.y, 
                  radius * 1.2, 0, 2 * std::numbers::pi);
        cairo_stroke(cr);
    }
    
    // Destination: Red circle
    if (destination_intersection.first >= 0) {
        cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.9);  // Bright red
        cairo_arc(cr, destination_intersection.second.x, destination_intersection.second.y, 
                  radius * 1.2, 0, 2 * std::numbers::pi);
        cairo_fill(cr);
        
        // Add white border
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_set_line_width(cr, 2.0 / g_view_state.zoom);
        cairo_arc(cr, destination_intersection.second.x, destination_intersection.second.y, 
                  radius * 1.2, 0, 2 * std::numbers::pi);
        cairo_stroke(cr);
    }
    
    // Clicked intersection: Blue circle (if different from origin/destination)
    if (clicked_intersection.first >= 0 && 
        clicked_intersection.first != origin_intersection.first &&
        clicked_intersection.first != destination_intersection.first) {
        cairo_set_source_rgba(cr, 0.0, 0.5, 1.0, 0.9);  // Sky blue
        cairo_arc(cr, clicked_intersection.second.x, clicked_intersection.second.y, 
                  radius * 1.2, 0, 2 * std::numbers::pi);
        cairo_fill(cr);
        
        // Add white border
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_set_line_width(cr, 2.0 / g_view_state.zoom);
        cairo_arc(cr, clicked_intersection.second.x, clicked_intersection.second.y, 
                  radius * 1.2, 0, 2 * std::numbers::pi);
        cairo_stroke(cr);
    }
    
    cairo_restore(cr);
}


void drawStreets(cairo_t* cr) {
    // Draw street network with proper colors, widths, arrows, and labels
    cairo_save(cr);
    
    // Set up default line properties
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    
    // Get visible world bounds for culling
    Rectangle& visible = g_view_state.visible_world;
    double x_min = visible.left();
    double y_max = visible.top();
    
    // Draw all street segments
    for (const auto& segment : globals.all_street_segments) {
        // Determine line width based on zoom level
        int line_width = -1;
        bool should_draw = false;
        
        for (const auto& zoom_pair : segment.zoom_levels) {
            if (current_zoom_level > zoom_pair.first) {
                line_width = zoom_pair.second;
                should_draw = true;
            }
        }
        
        // Skip if zoom level too low or outside visible area
        if (!should_draw || line_width == -1) {
            continue;
        }
        
        // Simple culling: check if segment average position is in visible area
        if (segment.x_avg <= x_min || segment.y_avg >= y_max) {
            continue;
        }
        
        // Draw street lines
        cairo_set_line_width(cr, line_width);
        
        // Set road color based on dark mode
        const GdkRGBA& road_color = globals.dark_mode ? segment.dark_road_colour : segment.road_colour;
        cairo_set_source_rgba(cr, road_color.red, road_color.green, road_color.blue, road_color.alpha);
        
        // Draw all line segments for this street
        for (const auto& line : segment.lines_to_draw) {
            cairo_move_to(cr, line.first.x, line.first.y);
            cairo_line_to(cr, line.second.x, line.second.y);
        }
        cairo_stroke(cr);
        
        // Draw one-way arrows if zoom level is sufficient
        if (current_zoom_level >= segment.arrow_zoom_dep && !segment.arrows_to_draw.empty()) {
            cairo_set_line_width(cr, segment.arrow_width);
            cairo_set_source_rgba(cr, 
                segment.arrow_colour.red, 
                segment.arrow_colour.green, 
                segment.arrow_colour.blue, 
                segment.arrow_colour.alpha);
            
            for (const auto& arrow : segment.arrows_to_draw) {
                cairo_move_to(cr, arrow.first.x, arrow.first.y);
                cairo_line_to(cr, arrow.second.x, arrow.second.y);
            }
            cairo_stroke(cr);
        }
        
        // Draw street name text if available
        if (!segment.text_to_draw.empty()) {
            const GdkRGBA& text_color = globals.dark_mode ? segment.dark_text_colour : segment.text_colour;
            cairo_set_source_rgba(cr, text_color.red, text_color.green, text_color.blue, text_color.alpha);
            
            // Create Pango layout for text
            PangoLayout* layout = pango_cairo_create_layout(cr);
            PangoFontDescription* font_desc = pango_font_description_from_string("Sans Bold 12");
            pango_layout_set_font_description(layout, font_desc);
            
            for (const auto& text : segment.text_to_draw) {
                cairo_save(cr);
                
                // Position and rotate text
                cairo_move_to(cr, text.loc.x, text.loc.y);
                cairo_rotate(cr, segment.text_rotation);
                
                pango_layout_set_text(layout, text.label.c_str(), -1);
                pango_cairo_show_layout(cr, layout);
                
                cairo_restore(cr);
            }
            
            pango_font_description_free(font_desc);
            g_object_unref(layout);
        }
    }
    
    cairo_restore(cr);
}



void draw_features(cairo_t *cr) {
    // Draw map features (parks, buildings, water bodies, etc.)
    cairo_save(cr);
    
    // Get visible world bounds for culling
    Rectangle& visible = g_view_state.visible_world;
    double x_min = visible.left();
    double x_max = visible.right();
    double y_min = visible.bottom();
    double y_max = visible.top();
    
    // Draw closed features (filled polygons)
    for (const auto& feature : closed_features) {
        // Skip features outside zoom level of detail
        if (current_zoom_level <= feature.zoom_lod) {
            continue;
        }
        
        // Cull features outside visible bounds
        if ((x_max < feature.x_min || x_min > feature.x_max) ||
            (y_max < feature.y_min || y_min > feature.y_max)) {
            continue;
        }
        
        // Skip if not enough points to draw
        if (feature.points.size() < 2) {
            continue;
        }
        
        // Set color based on dark mode
        const GdkRGBA& color = globals.dark_mode ? feature.dark_colour : feature.mycolour;
        cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
        
        // Draw polygon
        cairo_move_to(cr, feature.points[0].x, feature.points[0].y);
        for (size_t i = 1; i < feature.points.size(); ++i) {
            cairo_line_to(cr, feature.points[i].x, feature.points[i].y);
        }
        cairo_close_path(cr);
        cairo_fill(cr);
    }
    
    cairo_restore(cr);
}


void way_draw_features(cairo_t *cr) {
    // Draw OSM way features (trails, railways, etc.)
    cairo_save(cr);
    
    // Only draw if zoomed in enough
    if (current_zoom_level <= 4) {
        cairo_restore(cr);
        return;
    }
    
    // Set line properties
    cairo_set_line_width(cr, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    
    // Set grey color for way features (GREY_75 = 0.75, 0.75, 0.75)
    cairo_set_source_rgb(cr, 0.75, 0.75, 0.75);
    
    // Draw non-closed ways (trails, railways, etc.)
    for (const auto& way : m2_local_all_ways_info) {
        // Skip if not a drawable way type or insufficient points
        if (way.way_use == way_enums::notrail || way.way_points2d.size() < 2) {
            continue;
        }
        
        // Draw lines between consecutive points
        for (size_t j = 0; j < way.way_points2d.size() - 1; ++j) {
            cairo_move_to(cr, way.way_points2d[j].x, way.way_points2d[j].y);
            cairo_line_to(cr, way.way_points2d[j + 1].x, way.way_points2d[j + 1].y);
        }
    }
    
    cairo_stroke(cr);
    cairo_restore(cr);
}

void loadNewMap(const std::string& new_city,GtkApplication* application) {
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
    Point2D max_coord(max_x, max_y);
    Point2D min_coord(min_x, min_y);
    Rectangle new_coord(min_coord.x, min_coord.y, max_coord.x, max_coord.y);
    // TODO: GTK4 - Need to implement canvas coordinate change
    // application->change_canvas_world_coordinates("MainCanvas", new_coord);
    // application->refresh_drawing();
}