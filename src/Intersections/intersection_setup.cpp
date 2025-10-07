//
// Created by montinoa on 2/28/24.
//

#include "intersection_setup.hpp"
#include "../ms1helpers.h"
#include "m1.h"
#include "StreetsDatabaseAPI.h"

void fill_intersection_info() {
    globals.all_intersections.resize(getNumIntersections());
    for (int i = 0; i < getNumIntersections(); ++i) {
        
        double position_x, position_y;
        convertLatLonToXY(getIntersectionPosition(i), position_x, position_y, globals.map_lat_avg);
        
        globals.all_intersections[i].position = ezgl::point2d(position_x, position_y);
        globals.all_intersections[i].id = getIntersectionOSMNodeID(i);
        globals.all_intersections[i].name = getIntersectionName(i);
        globals.all_intersections[i].index = i;
    }
}