//
// Created by montinoa on 2/28/24.
//

#include "intersection_setup.hpp"
#include "../Coordinates_Converstions/coords_conversions.hpp"
#include "../binary_loader/binary_database.hpp"
#include "../struct.h"

// Local static storage for intersection data (replaces globals.all_intersections)
static std::vector<intersection_info> all_intersections;

void fill_intersection_info() {
    // Get the BinaryDatabase instance
    auto& db = gisevo::BinaryDatabase::instance();
    
    // Get map latitude average in radians for coordinate conversion
    double map_lat_avg_rad = db.get_avg_lat_rad();
    
    // Get number of intersections from the database
    size_t num_intersections = db.get_intersection_count();
    
    // Resize the local intersections vector
    all_intersections.resize(num_intersections);
    
    for (size_t i = 0; i < num_intersections; ++i) {
        // Get intersection position as LatLon
        LatLon intersection_pos = db.get_intersection_position(i);
        
        // Convert to Point2D using the new coordinate conversion function
        Point2D position = latlonTopoint(intersection_pos, map_lat_avg_rad);
        
        // Fill intersection info
        all_intersections[i].position = position;
        all_intersections[i].id = db.get_intersection_osm_node_id(i);
        all_intersections[i].name = db.get_intersection_name(i);
        all_intersections[i].index = i;
    }
}