//
// Created by montinoa on 2/28/24.
//

#pragma once

#include "LatLon.h"
#include "m1.h"
#include <gtk/gtk.h>
#include <cairo.h>
#include "../gtk4_types.hpp"



// Converstion Functions
LatLon xy_to_latlon();
double lon_to_x(double longitude);
double lat_to_y(double latitude);
double y_to_lat(double y);
double x_to_lon(double x);
Point2D latlonTopoint(LatLon latlon);
// Coordinates Functions
/*
 *
 */
double find_map_bounds();

// Zoom Level
void get_current_zoom_level(double& x_zoom_prev, double& y_zoom_prev, int& current_zoom_level, Rectangle visible_world);
