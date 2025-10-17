//
// Created by montinoa on 2/28/24.
//

#include "coords_conversions.hpp"
#include "StreetsDatabaseAPI.h"
#include <limits>

double find_map_bounds() {
    double max_lat = std::numeric_limits<double>::lowest();
    double max_lon = std::numeric_limits<double>::lowest();
    double min_lat = std::numeric_limits<double>::max();
    double min_lon = std::numeric_limits<double>::max();
    
    // Use StreetsDatabaseAPI instead of global variables
    unsigned num_intersections = getNumIntersections();
    for (unsigned i = 0; i < num_intersections; ++i) {
        LatLon coords = getIntersectionPosition(i);
        if (coords.latitude() > max_lat) {
            max_lat = coords.latitude();
        }
        if (coords.latitude() < min_lat) {
            min_lat = coords.latitude();
        }
        if (coords.longitude() > max_lon) {
            max_lon = coords.longitude();
        }
        if (coords.longitude() < min_lon) {
            min_lon = coords.longitude();
        }
    }

    // Return average latitude in radians
    return ((min_lat + max_lat)/2) * coords::kDegreeToRadian;
}
