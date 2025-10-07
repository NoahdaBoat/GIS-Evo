#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <unordered_map>
#include <string>
#include "m1.h"
#include "sort_streetseg/streetsegment_info.hpp"
#include "StreetsDatabaseAPI.h"
#include "OSMDatabaseAPI.h"
#include "struct.h"
#include "ezgl/graphics.hpp"


class Global_Var {
public:

    // this hold the strings of all the map paths, and if they are already open or closed as a boolean
    std::unordered_map<std::string, bool> loadedMap;

    // vector of vectors of street segment id organized by intersection id
    std::vector<std::vector<StreetSegmentIdx>> intersection_street_segments;

    // multimap of street names in alphabetical order (key: name, data: street ID)
    std::multimap<std::string, StreetIdx> ordered_street_name;

    // this string holds the path of the current map file
    std::string current_map_open;

    // vector of struct streetinfo, organized by street id
    std::vector<StreetsInfo> vec_streetinfo;

    // holds the total distance corresponding with each OSMWay
    std::unordered_map <OSMID, double> way_distance;

    // relates a given intersection, with a vector of all adjacent intersections
    std::unordered_map<IntersectionIdx, std::vector<IntersectionIdx>> adjacent_intersections;

    // used to find any OSMNode given an OSMID
    std::unordered_map<OSMID, const OSMNode*> node_to_id;

    // used to find any OSMWay given an OSMID
    std::unordered_map<OSMID, const OSMWay*> id_to_way;

    // used to find any OSMRelation given an OSMID
    std::unordered_map<OSMID, const OSMRelation*> id_to_relation;

    // vector of struct streetSegmentDistance, organized by street segment id
    std::vector<StreetSegmentDistance> vec_segmentdis;

    // The following values are the maximum and minimum longitudes for the current map, as well as the average latitude
    double max_lat, min_lat, max_lon, min_lon, map_lat_avg;

    // This is a vector of all intersections, along with their data, for easy access
    std::vector<intersection_info> all_intersections;

    // This is a custommed class containing vectors of categorized POI
    POI_sorted poi_sorted;

    // vector of unordered_maps containing image name to surface pointer 
    Vec_Png vec_png;

    // an unordered map with key being country names, each country name correspond to an unordered map with city name as key and map path as data
    std::unordered_map<std::string,std::unordered_map<std::string,std::string>> map_names;

    // goes from map path to map name
    std::vector<std::pair<std::string, City_Country>> map_path_to_name;

    // Vector of restaurants in the city
    std::vector<internet_poi> city_restaurants;

    // Vector of top 30 shops in the city
    std::vector<internet_poi> city_shops;

    // Holds a struct of all street segments in the city
    std::vector<street_segment_info> all_street_segments;

    std::vector<RoadType> ss_road_type;

    std::vector<bool> draw_which_poi;

    bool dark_mode = false;

    // maximum speed of the region loaded
    float max_speed;

    std::unordered_map<IntersectionIdx ,Delivery_Stop> delivery_stops;

    std::unordered_map<IntersectionIdx ,Delivery_details> delivery_info;
};

extern Global_Var globals;
