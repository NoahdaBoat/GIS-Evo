//
// Created by montinoa on 2/28/24.
//

#include "coords_conversions.hpp"
#include <iostream>
#include "../ezgl/rectangle.hpp"

void get_current_zoom_level(double& x_zoom_prev, double& y_zoom_prev, int& current_zoom_level, ezgl::renderer *g) {
    ezgl::rectangle current_zoom_rectangle = g->get_visible_world();
//    std::cout << current_zoom_rectangle.m_first.x - current_zoom_rectangle.m_second.x << std::endl;
//    std::cout << current_zoom_rectangle.m_second.y - current_zoom_rectangle.m_first.y << std::endl;
    double x_zoom = (current_zoom_rectangle.m_first.x - current_zoom_rectangle.m_second.x);
    double y_zoom = (current_zoom_rectangle.m_second.y - current_zoom_rectangle.m_first.y);
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