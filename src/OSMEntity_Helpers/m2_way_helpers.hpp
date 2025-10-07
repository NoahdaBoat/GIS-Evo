//
// Created by montinoa on 2/28/24.
//

#pragma once

#include <string>
#include <vector>
#include "m1.h"
#include "../ms1helpers.h"
#include "../ms2helpers.hpp"
#include "../sort_streetseg/streetsegment_info.hpp"
#include "ezgl/point.hpp"
#include "typed_osmid_helper.hpp"
#include "/cad2/ece297s/public/include/streetsdatabase/StreetsDatabaseAPI.h"
#include "OSMDatabaseAPI.h"


/*
 * This header file is for m2_way_helpers.cpp
 * It contains the definitions for all m2 code related to dealing with OSMWays
 */

// Classes

// Structs

/*
 *
 */
struct way_info {
    bool is_closed;
    std::vector <ezgl::point2d> way_points2d;
    OSMID way_id;
    //double length;
    std::string way_name;
    FeatureType way_type;
    RoadType way_road_type;
    way_enums way_use;
};

struct feature_info {
    std::vector<ezgl::point2d> points;
    FeatureType type;
    std::string feature_name;
    TypedOSMID id;
    double x_max, x_min, y_max, y_min, x_avg, y_avg;
    ezgl::color mycolour;
    ezgl::color dark_colour;
    int zoom_lod;
};

// Global Variables
extern std::vector<way_info> m2_local_all_ways_info;
extern std::vector<feature_info> closed_features;
extern std::vector<feature_info> open_features;
//extern std::unordered_map<uint, std::vector<feature_info>> spatial_hash;
extern std::vector<std::vector<feature_info>> spatial_hash;
extern std::vector<feature_info> always_draw;
/*
 *
 */

// Enums

// Functions

/*
 *
 */
std::vector<way_info> create_vector_of_ways(std::unordered_map<OSMID, feature_data*>& id_to_way);


/*
 *
 */
bool compare_structs(const way_info& structure1, const way_info& structure2);

std::unordered_map<OSMID, int> map_way_to_idx();

void assign_type_to_way();

void sort_features();

bool set_lod_feature(int zoom_level, FeatureType type);

//converts a string to color
ezgl::color stringToRgb(std::string& colour_str);

