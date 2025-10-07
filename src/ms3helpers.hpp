//
// Created by helanlin on 3/22/24.
//

#ifndef MAPPER_MS3HELPERS_HPP
#define MAPPER_MS3HELPERS_HPP
#include "m3.h"
#include "ezgl/graphics.hpp"
#include "ezgl/application.hpp"

enum Directions{
    RIGHT = 0,
    LEFT,
    STRAIGHT,
    U_turn
};

/*
 * highlight the found route
 */
void highlightRoute (ezgl::renderer* g, const std::vector<StreetSegmentIdx>& path);

/*
 * find the max and min point in the route for auto-zoom
 */
std::pair<ezgl::point2d,ezgl::point2d> findMaxMinPoint(const std::vector<StreetSegmentIdx> & route);

/*
 * Clear and revert all the parameters back so only draw arrows for one-way street
 */
void clearRoadArrows(const std::vector<StreetSegmentIdx>& route);

/*
 * Go in street_segment_info and store information so that arrows for found route will be drawn during draw_main-canvas
 */
void drawRoadArrows(const std::vector<StreetSegmentIdx>& route,int current_zoom_level, IntersectionIdx src);

/*
 * Draw arrows and street names again on top of the highlighted route
 */
void redrawStreetComponents(ezgl::renderer *g, const std::vector<StreetSegmentIdx>& route);

/*
 * Return a vector of strings of the directions between each street segment (Left/right/straight)
 */
std::vector<std::string> findDirections(const std::vector<IntersectionIdx>& route);

/*
 * Helper functoins for findDirections that determines the direction between two street segments
 */
Directions findAngleSegments(StreetSegmentIdx from, StreetSegmentIdx to);

/*
 *
 */
void searchRouteToggle(GtkToggleButton* search_route_toggle, ezgl::application* application);

#endif //MAPPER_MS3HELPERS_HPP
