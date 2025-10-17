#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include "StreetsDatabaseAPI.h"
#include "gtk4_types.hpp"
#include "ezgl/graphics.hpp"
#include "POI/POI_setup.hpp"

struct StreetsInfo {
    // contains all the intersections of the street
    std::vector <IntersectionIdx> intersections;
    // contatins all the street segment of the street
    std::vector <StreetSegmentIdx> street_segments;
    // length of the street
    double street_length;
};

struct StreetSegmentDistance{
    // length of street segment
    double segment_length;
    // travel time if traveling at speed limit
    double travel_time;
};

class POI_info{
    public:
    Point2D poi_loc;
    Point2D poi_text_loc;
    std::string poi_name;
    POIIdx poi_idx;
    POI_class poi_class;
    int poi_customed_type;
    POI_category poi_category;

    //constructor
    POI_info(Point2D loc, Point2D text_loc, std::string name, POIIdx idx, POI_class input_class,POI_category category):poi_loc(loc), poi_text_loc(text_loc), poi_name(name), poi_idx(idx),poi_class(input_class),poi_category(category)
    {
    }
    POI_info(){}

};

struct POI_sorted{
    std::vector<std::vector<POI_info>> basic_poi;
    std::vector<std::vector<POI_info>> entertainment_poi;
    std::vector<std::vector<POI_info>> subordinate_poi;
    std::vector<POI_info> neglegible_poi;
    std::vector<POI_info> stations_poi;
};

struct intersection_info {
    Point2D position;
    std::string name;
    IntersectionIdx index;
    OSMID id;
    bool highlight = false;
};

struct Vec_Png{
    std::vector<cairo_surface_t*> zoom_out;
    std::vector<cairo_surface_t*> zoom_in;
};

struct City_Country{
    std::string city_name;
    std::string country_name;
};

struct Delivery_Stop{
    int stop_type;
    int delivery_index;
    IntersectionIdx pickUp;
    IntersectionIdx dropOff;
    Delivery_Stop(int type,int id, IntersectionIdx index):stop_type(type),delivery_index(id),pickUp(0),dropOff(0){
        if (type == 0 || type == 1) pickUp = index; // DEPOT=0 or PICKUP=1
        else dropOff = index;
    };
    Delivery_Stop():stop_type(0),delivery_index(0),pickUp(0),dropOff(0){};
};

typedef Delivery_Stop DeliveryInf;

class Delivery_details {
public:
    bool pick_up;
    bool drop_off;
    bool visited;
    bool added;
    std::vector<IntersectionIdx> corres_pickup;
    std::vector<IntersectionIdx> corres_dropoff;
    Delivery_details(): pick_up(false),drop_off(false),visited(false),added(false){};
    explicit Delivery_details(bool if_pick, bool if_drop, bool if_visited): pick_up(if_pick),drop_off(if_drop),visited(if_visited),added(false){};
};


