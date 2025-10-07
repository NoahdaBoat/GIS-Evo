//
// Created by montinoa on 2/28/24.
//

#pragma once

#include <vector>
#include <string>
#include "m1.h"
#include "../ms1helpers.h"
#include "../ezgl/graphics.hpp"
#include "OSMDatabaseAPI.h"



// Classes

// Enums

enum typed_id_types {
    Way,
    Node,
    Relation
};

enum way_enums {
    tram,
    train,
    subway,
    monorail,
    otherrail,
    tracks,
    notrail
};

// Structs

//
class each_relation {
public:
    std::vector<TypedOSMID> members;
    TypedOSMID::EntityType type;
    std::vector<std::string> relation_roles;
    std::string name;
};

struct feature_data {
    TypedOSMID id;
    std::string feature_name;
    FeatureType type;
};

//
class relation_info {
public:
    OSMID id;
    std::unordered_map<const OSMEntity*, typed_id_types> relation_members;
    std::vector<std::string> relation_roles;
};

class subway_info:public each_relation{
public:
    std::string colour;
    std::vector<std::vector<ezgl::point2d>> subway_way;
    ezgl::color draw_colour;
};
// Globals

// used to store all relations in a single vector for easy access
extern std::vector<relation_info> m2_local_all_relations_info;
extern std::vector<feature_data> m2_local_all_features_info;
extern std::unordered_map<OSMID, feature_data*> m2_local_id_to_feature;
extern std::vector<each_relation> m2_local_all_relations_vector;
extern std::vector<subway_info> subway_lines;

// Functions

/*
 *
 */
void sort_relations();

/*
 *
 */

/*
 *
 */
std::vector<each_relation> create_vector_entities(std::vector<relation_info> all_relations);

std::unordered_map<OSMID, feature_data*> map_features_to_ways(std::vector<feature_data>& all_features);

bool compare_ids(OSMID& way_id, feature_data& s1);

void set_colour_of_feature(ezgl::color mycolour, FeatureType way_type);

std::vector<each_relation> sort_osmrelations();

bool compare_relation_names(each_relation e1);

//initalize the global variable that contains all of the subway stations
void initSubwayStations();

//sort and initialize all of the subway lines as global variable
void sortSubwayLines();



