//
// Created by montinoa on 2/28/24.
//

#pragma once

#include "LatLon.h"
#include "../gtk4_types.hpp"

// Coordinate conversion constants
namespace coords {
    constexpr double kDegreeToRadian = 0.017453292519943295; // PI / 180
    constexpr double kEarthRadiusInMeters = 6371000.0;
}



// Converstion Functions
LatLon xy_to_latlon();
double lon_to_x(double longitude, double map_lat_avg_rad);
double lat_to_y(double latitude);
double y_to_lat(double y);
double x_to_lon(double x, double map_lat_avg_rad);
Point2D latlonTopoint(LatLon latlon, double map_lat_avg_rad);
// Coordinates Functions
/*
 *
 */
double find_map_bounds();

// Zoom Level
void get_current_zoom_level(double& x_zoom_prev, double& y_zoom_prev, int& current_zoom_level, Rectangle visible_world);
