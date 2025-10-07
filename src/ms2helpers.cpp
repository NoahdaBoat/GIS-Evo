//
// Created by montinoa on 2/19/24.
//

#include <iostream>
#include <vector>
#include <algorithm>
#include <limits>
#include <execution>
#include <unordered_map>

#include "m1.h"
#include "ms1helpers.h"
#include "ms2helpers.hpp"
#include "OSMDatabaseAPI.h"

#include "Coordinates_Converstions/coords_conversions.hpp"
#include "ezgl/point.hpp"
#include "ezgl/graphics.hpp"
#include "OSMEntity_Helpers/typed_osmid_helper.hpp"


void combo_box_cbk(GtkComboBoxText* self, ezgl::application* app){
    //Getting text content of combo box. This call makes a copy that we must free
    auto text = gtk_combo_box_text_get_active_text(self);
    
    //Returning if the combo box is currently empty (Always check to avoid errors)
    if(!text){  
        return;
    } 
    // Updating message to reflect new combo box value.
    else {  
        app->update_message(text);

        // gtk made a copy that we own, need to free
        g_free (text);      
    }
}


void GtkTextEntry(GtkWidget*, ezgl::application* application) {
    GObject* gtk_object = application->get_object("TextEntry");
    GtkEntry * gtk_entry = GTK_ENTRY(gtk_object);
    const gchar* text = gtk_entry_get_text(gtk_entry);
    application->update_message(text);
    application->refresh_drawing();
}


void load_image_files(){
    // populate zoom-in pngs
    ezgl::surface* basic_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_in/basic.png");
    ezgl::surface* entertainment_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_in/entertainment.png");
    ezgl::surface* food_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_in/food.png");
    ezgl::surface* shopping_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_in/shopping.png");
    ezgl::surface* subordinate_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_in/subordinate.png");
    ezgl::surface* neglect_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_in/neglect.png");
    ezgl::surface* pin_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_in/pin.png");

    globals.vec_png.zoom_in.resize(20);
    globals.vec_png.zoom_in[POI_category::BASIC] = basic_surface;
    globals.vec_png.zoom_in[POI_category::ENTERTAINMENT] = entertainment_surface;
    globals.vec_png.zoom_in[POI_category::FOOD] = food_surface;
    globals.vec_png.zoom_in[POI_category::SHOPPING] = shopping_surface;
    globals.vec_png.zoom_in[POI_category::SUBORDINATE] = subordinate_surface;
    globals.vec_png.zoom_in[POI_category::NEGLECT] = neglect_surface;
    globals.vec_png.zoom_in[POI_category::PIN] = pin_surface;

    // populate zoom-out png
    ezgl::surface* dot_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_out/dot.png");
    ezgl::surface* ent_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_out/ent.png");
    ezgl::surface* food_bev_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_out/food_bev.png");
    ezgl::surface* drinks_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_out/drinks.png");
    ezgl::surface* hospital_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_out/hospitals.png");
    ezgl::surface* subway_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_out/subway.png");
    ezgl::surface* grocery_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_out/grocery.png");
    ezgl::surface* highlight_surface = ezgl::renderer::load_png("libstreetmap/resources/zoom_out/highlight.png");


    globals.vec_png.zoom_out.resize(20);
    globals.vec_png.zoom_out[POI_category::BASIC] = dot_surface;
    globals.vec_png.zoom_out[POI_category::ENTERTAINMENT] = ent_surface;
    globals.vec_png.zoom_out[POI_category::FOOD] = food_bev_surface;
    globals.vec_png.zoom_out[POI_category::DRINK] = drinks_surface;
    globals.vec_png.zoom_out[POI_category::HOSPITAL] = hospital_surface;
    globals.vec_png.zoom_out[POI_category::SUBWAY] = subway_surface;
    globals.vec_png.zoom_out[POI_category::GROCERY] = grocery_surface;
    globals.vec_png.zoom_out[POI_category::HIGHLIGHT] = highlight_surface;
}


void drawPOIType (ezgl::renderer *g,  std::vector<POI_info>& inner_vector, POI_category poi_category,double scale, double x1, double x2, double y1, double y2) {
    for (int inner = 0; inner < (inner_vector.size()); inner++) {
        ezgl::point2d anchor = inner_vector[inner].poi_loc;
        if(anchor.x<x1 && anchor.x>x2 && anchor.y < y1 && anchor.y>y2) {
            g->draw_surface(globals.vec_png.zoom_out[poi_category], anchor, scale);
        }
    }
}


void loadMapNames(){
    City_Country city_name_array []={"beijing","china","boston","usa","cape-town","south-africa","golden_horseshoe","canada","hamilton","canada","hong-kong","china","iceland","iceland","interlaken","switzerland","kyiv","ukraine","london","england","new-delhi","india","new-york","usa","rio-de-janeiro","brazil","saint-helena","saint-helena","singapore","singapore","sydney","australia","tehran","iran","tokyo","japan","toronto","canada"};
    std::string map_path[]={"/cad2/ece297s/public/maps/beijing_china.streets.bin","/cad2/ece297s/public/maps/boston_usa.streets.bin","/cad2/ece297s/public/maps/cape-town_south-africa.streets.bin","/cad2/ece297s/public/maps/golden-horseshoe_canada.streets.bin","/cad2/ece297s/public/maps/golden-horseshoe_canada.streets.bin","/cad2/ece297s/public/maps/hong-kong_china.streets.bin","/cad2/ece297s/public/maps/iceland.streets.bin","/cad2/ece297s/public/maps/interlaken_switzerland.streets.bin","/cad2/ece297s/public/maps/kyiv_ukraine.streets.bin","/cad2/ece297s/public/maps/london_england.streets.bin","/cad2/ece297s/public/maps/new-delhi_india.streets.bin","/cad2/ece297s/public/maps/new-york_usa.streets.bin","/cad2/ece297s/public/maps/rio-de-janeiro_brazil.streets.bin","/cad2/ece297s/public/maps/saint-helena.streets.bin","/cad2/ece297s/public/maps/singapore.streets.bin","/cad2/ece297s/public/maps/sydney_australia.streets.bin","/cad2/ece297s/public/maps/tehran_iran.streets.bin","/cad2/ece297s/public/maps/tokyo_japan.streets.bin","/cad2/ece297s/public/maps/toronto_canada.streets.bin"};
    for(int i = 0; i<19; i++){
        globals.map_names[city_name_array[i].country_name][city_name_array[i].city_name] = map_path[i];
    }
    globals.map_path_to_name.resize(19);
    for (uint i = 0; i < 19; ++i) {
        globals.map_path_to_name[i].first = map_path[i];
        globals.map_path_to_name[i].second = city_name_array[i];
    }
}

int searchCityCountry(std::string& country,std::unordered_map<std::string,std::string>& found_cities){
    // pre-process input
    lowerCase(country);
    country.erase(std::remove(country.begin(), country.end(), ' '),country.end());
    // check if the country exist
    if(globals.map_names.count(country)==0){
        return -1;
    }
    found_cities = globals.map_names[country];
    return 0;
}


bool draw_streets_check(int zoom_level, RoadType type) {
    bool to_draw = false;
    switch (type) {
        case RoadType::motorway:
        case RoadType::motorway_link:
        case RoadType::trunk:
        case RoadType::trunk_link:
            if (zoom_level > -5) {
                to_draw = true;
            }
            break;

        case RoadType::primary:
        case RoadType::primary_link:
            if (zoom_level > 2) {
                to_draw = true;
            }
            break;

        case RoadType::secondary:
        case RoadType::secondary_link:
            if (zoom_level > 3) {
                to_draw = true;
            }
            break;

        case RoadType::tertiary:
        case RoadType::tertiary_link:
            if (zoom_level > 4) {
                to_draw = true;
            }
            break;

        case RoadType::road:
            if (zoom_level > 4) {
                to_draw = true;
            }
            break;

        case RoadType::service:
            if (zoom_level > 8) {
                to_draw = true;
            }
            break;

        case RoadType::footway:
        case RoadType::path:
        case RoadType::bridleway:
        case RoadType::trail:
        case RoadType::pedestrian:
            if (zoom_level > 9) {
                to_draw = true;
            }
            break;

        case RoadType::cycleway:
            if (zoom_level > 9) {
                to_draw = true;
            }
            break;

        case RoadType::residential:
        case RoadType::living_street:
            if (zoom_level > 7) {
                to_draw = true;
            }
            break;

        default:
            if (zoom_level > 7) {
                to_draw = true;
            }
            break;
    }
    return to_draw;
}


void drawPOIName(ezgl::renderer *g,POI_class drawing_class, double text_scale,double num_scale,ezgl::point2d increment,double x_max, double x_min, double y_max,double y_min){
    auto *drawing_vec = &globals.poi_sorted.basic_poi;
    auto *station_neglect =&globals.poi_sorted.stations_poi;
    bool inner_loop = true;
    //check for which vector is being looped
    switch(drawing_class) {
        case POI_class::station:
            inner_loop = false;
            break;
        case POI_class::basic:
            drawing_vec = &globals.poi_sorted.basic_poi;
            break;
        case POI_class::entertainment:
            drawing_vec = &globals.poi_sorted.entertainment_poi;
            break;
        case POI_class::subordinate:
            drawing_vec = &globals.poi_sorted.subordinate_poi;
            break;
        case POI_class::neglegible:
            station_neglect = &globals.poi_sorted.neglegible_poi;
            inner_loop = false;
            break;

        default:
            station_neglect = &globals.poi_sorted.neglegible_poi;
            inner_loop = false;
            break;
    }
    // if an inner loop is required (i.e. anything but subway and neglect)
    if(inner_loop){
        for(const auto& poi_class:*drawing_vec){
            for(int i = 0; i < poi_class.size()*num_scale; i++){
                ezgl::point2d poi_loc =poi_class[i].poi_text_loc;
                //only draw when it is visible
                if(poi_loc.x <x_max && poi_loc.x >x_min && poi_loc.y < y_max && poi_loc.y > y_min) {
                    std::string poi_name = poi_class[i].poi_name;
                    poi_loc += increment;
                    g->set_text_rotation(0.0);
                    g->set_font_size(text_scale);
                    g->set_color(ezgl::BLACK);
                    g->draw_text(poi_loc, poi_name);
                }
            }
        }
    }
    else{
        //no inner loop needed (i.e drawing neglect or subway stations
        for (const auto &poi: *station_neglect) {
            ezgl::point2d poi_loc = poi.poi_text_loc;
            if (poi_loc.x < x_max && poi_loc.x > x_min && poi_loc.y < y_max && poi_loc.y > y_min) {
                std::string poi_name = poi.poi_name;
                poi_loc += increment;
                g->set_text_rotation(0.0);
                if (drawing_class == station) {
                    g->set_color(ezgl::DARK_SLATE_BLUE, 255);
                    g->format_font(" ", ezgl::font_slant::normal, ezgl::font_weight::bold);
                    g->set_font_size(text_scale * 1.2);
                } 
                else {
                    g->set_color(ezgl::BLACK);
                    g->set_font_size(text_scale);
                }
                g->draw_text(poi_loc, poi_name);
                g->format_font(" ", ezgl::font_slant::normal, ezgl::font_weight::normal);
            }
        }
    }
}


std::string getPathCity(std::string city, std::unordered_map<std::string, std::string> list_cities){
    //pre-process input
    lowerCase(city);
    city.erase(std::remove(city.begin(), city.end(), ' '),city.end());
    if(list_cities.count(city) == 0){
        return " ";
    }
    std::string path = list_cities[city];
    return path;
}


void setAllBool(bool state){
    for(auto boolean : globals.draw_which_poi){
        boolean = state;
    }
}


void drawSubwayLines(ezgl::renderer *g){
    //loop through all subway lines which contains vector of multiple ways
    for(int line =0; line < subway_lines.size(); line++){
        std::vector<std::vector<ezgl::point2d>> vec_ways = subway_lines[line].subway_way;
        //loop through each ways of current line
        for(int way_idx = 0; way_idx <vec_ways.size();way_idx++){
            for(int node = 0; node < vec_ways[way_idx].size()-1; node++) {
                g->set_line_width(5);
                g->set_color(subway_lines[line].draw_colour);
                g->draw_line(vec_ways[way_idx][node], vec_ways[way_idx][node + 1]);
            }
        }
    }
}