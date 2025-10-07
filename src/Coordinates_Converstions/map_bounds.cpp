//
// Created by montinoa on 2/28/24.
//

#include "m1.h"
#include "../ms1helpers.h"
#include "coords_conversions.hpp"

double find_map_bounds() {
    globals.max_lat = std::numeric_limits<double>::lowest();
    globals.max_lon = std::numeric_limits<double>::lowest();
    globals.min_lat = std::numeric_limits<double>::max();
    globals.min_lon = std::numeric_limits<double>::max();
    for (int i = 0; i < getNumberOfNodes(); ++i) {
        const OSMNode* currentNode = getNodeByIndex(i);
        LatLon coords = getNodeCoords(currentNode);
        if (coords.latitude() > globals.max_lat) {
            globals.max_lat = coords.latitude();
        }
        if (coords.latitude() < globals.min_lat) {
            globals.min_lat = coords.latitude();
        }
        if (coords.longitude() > globals.max_lon) {
            globals.max_lon = coords.longitude();
        }
        if (coords.longitude() < globals.min_lon) {
            globals.min_lon = coords.longitude();
        }
    }


    return ((globals.min_lat + globals.max_lat)/2) * kDegreeToRadian;
}
