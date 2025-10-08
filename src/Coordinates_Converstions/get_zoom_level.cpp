//
// Created by montinoa on 2/28/24.
//

#include "coords_conversions.hpp"
#include <iostream>
#include "../gtk4_types.hpp"

void get_current_zoom_level(double& x_zoom_prev, double& y_zoom_prev, int& current_zoom_level, Rectangle visible_world) {
    // TODO: GTK4 - Update to use Rectangle instead of renderer
    (void)visible_world; // Suppress unused parameter warning
    
    // Original implementation calculated zoom from rectangle dimensions
    // Need to adapt to use Rectangle visible_world directly
    double x_zoom = (visible_world.x1 - visible_world.x2);
    double y_zoom = (visible_world.y2 - visible_world.y1);
    double scale_factor_x = 0;
    double scale_factor_y = 0;
    bool first = false;
    bool first_x = false;
    bool first_y = false;
    if (x_zoom_prev != 0) {
        scale_factor_x = x_zoom/x_zoom_prev;
    }
    else {
        first_x = true;
    }
    if (y_zoom_prev != 0) {
        scale_factor_y = y_zoom/y_zoom_prev;
    }
    else {
        first_y = true;
    }
    if (first_x && first_y) {
        first = true;
    }

    if (x_zoom != x_zoom_prev && y_zoom != y_zoom_prev && (((scale_factor_x - scale_factor_y) > -0.01) && ((scale_factor_y - scale_factor_x) < 0.01) ) && ((scale_factor_x != 0 && scale_factor_y != 0) || first)) {
        if (x_zoom < x_zoom_prev && y_zoom > y_zoom_prev && scale_factor_x > 0.7) {
            current_zoom_level--;
        }
        else {
            current_zoom_level++;
        }
        x_zoom_prev = x_zoom;
        y_zoom_prev = y_zoom;
    }
}