#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "ezgl/graphics.hpp"
#include "OSMDatabaseAPI.h"

enum RoadType {
    primary = 0,
    residential,
    tertiary,
    service,
    motorway,
    motorway_link,
    trunk,
    secondary,
    trunk_link,
    primary_link,
    secondary_link,
    living_street,
    footway,
    pedestrian,
    unclassified,
    cycleway,
    path,
    tertiary_link,
    bridleway,
    //raceway,
    trail,
    road,
    other
};

struct text_prop {
    ezgl::point2d loc;
    std::string label;
    double length_x;
    double length_y;
};

struct street_segment_info {
    StreetIdx street;
    std::string street_name;
    std::string inter_to;
    std::string inter_from;
    int num_curve_point;
    ezgl::point2d max_pos;
    ezgl::point2d min_pos;
    StreetSegmentIdx index;
    IntersectionIdx to;
    IntersectionIdx from;
    bool oneWay;
    double speedLimit;
    OSMID id;
    RoadType type;
    ezgl::color road_colour;
    ezgl::color dark_road_colour;
    ezgl::color arrow_colour;
    ezgl::color text_colour;
    ezgl::color dark_text_colour;
    double x_avg;
    double y_avg;
    int arrow_width;
    std::vector<std::pair<ezgl::point2d, ezgl::point2d>> lines_to_draw;
    std::vector<std::pair<ezgl::point2d, ezgl::point2d>> arrows_to_draw;
    std::vector<text_prop> text_to_draw;
    double text_rotation;
    std::vector<std::pair<int, int>> zoom_levels;
    int arrow_zoom_dep;
};

extern std::vector<RoadType> m2_local_all_street_types;

void draw_arrows(int idx, ezgl::point2d from, ezgl::point2d to);

double calculate_angle(double from_pos_x, double from_pos_y, double to_pos_x, double to_pos_y);

void set_colour_of_street(RoadType type, int idx);

void compute_streets_info();

