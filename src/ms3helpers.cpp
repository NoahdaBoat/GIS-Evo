//
// Created by helanlin on 3/22/24.
//

#include "m3.h"
#include "m1.h"
#include "ms3helpers.hpp"
#include "globals.h"
#include "ezgl/graphics.hpp"
#include "coords_conversions.hpp"
#include <algorithm>
#include <cmath>

void highlightRoute (ezgl::renderer* g, const std::vector<StreetSegmentIdx>& path){
    //loop through each segment and draw a thick line on top
    g->set_line_width(10);
    g->set_color(100,149,237);
    if(path.empty()){
        return;
    }
    for(const auto segment : path){
        for(auto point : globals.all_street_segments[segment].lines_to_draw){
            g->draw_line(point.first,point.second);
        }
    }
}

std::pair<ezgl::point2d,ezgl::point2d> findMaxMinPoint(const std::vector<StreetSegmentIdx>& route){
    double max_x = -1 * std::numeric_limits<double>::max();
    double max_y = -1 * std::numeric_limits<double>::max();
    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();

    // loop through all of the street segment, get their individual max and min, compare to obtain absolute max and min of the route
   for (auto& segment : route) {
      max_x = std::max(max_x, globals.all_street_segments[segment].max_pos.x);
      max_y = std::max(max_y, globals.all_street_segments[segment].max_pos.y);
      min_x = std::min(min_x, globals.all_street_segments[segment].min_pos.x);
      min_y = std::min(min_y, globals.all_street_segments[segment].min_pos.y);
   }

   ezgl::point2d max(max_x, max_y);
   ezgl::point2d min(min_x, min_y);
   std::pair<ezgl::point2d,ezgl::point2d> max_min = std::make_pair(max, min);
   return max_min;
}

void drawRoadArrows(const std::vector<StreetSegmentIdx>& route,int current_zoom_level, IntersectionIdx src) {

    //check if it is going from "from to to" or "to to from" direction
    IntersectionIdx prev_inter = globals.all_street_segments[route[0]].from;
    bool from_to_to = false;
    if(src == globals.all_street_segments[route[0]].from){
        from_to_to =true;
        prev_inter = globals.all_street_segments[route[0]].to;
    }

    //loop through all segments of the route
    for(int i =0; i< route.size(); i++){
        StreetSegmentIdx segment = route[i];
        street_segment_info info = globals.all_street_segments[segment];
        info.arrow_width = 5;
        info.arrow_zoom_dep = current_zoom_level;
        if(i!=0) {
            // check for directions (from -> to or to -> from)
            if (info.from == prev_inter) {
                from_to_to = true;
                prev_inter = info.to;
            }
            else {
                from_to_to = false;
                prev_inter = info.from;
            }
        }
        //only add in arrows if it is not a one way street
        if(!info.oneWay) {
            if (info.num_curve_point == 0) {
                if(from_to_to) {
                    draw_arrows(segment, globals.all_intersections[info.from].position,
                                globals.all_intersections[info.to].position);
                }
                else{
                    //contains curve points
                    draw_arrows(segment, globals.all_intersections[info.to].position,
                                globals.all_intersections[info.from].position);
                }
            }
            else {
                // to -> from direction
                for (int j = 0; j < info.lines_to_draw.size() - 1; j++) {
                    if(from_to_to) {
                        draw_arrows(segment, info.lines_to_draw[j].first, info.lines_to_draw[j].second);
                    }
                    else{
                        draw_arrows(segment, info.lines_to_draw[j].second, info.lines_to_draw[j].first);
                    }
                }
            }
        }
    }
}

void clearRoadArrows(const std::vector<StreetSegmentIdx>& route){
    for(auto segment:route){
        street_segment_info info = globals.all_street_segments[segment];
        info.arrow_width = 1;
        info.arrow_zoom_dep = 9;
        //only add in arrows if it is not a one way street
        if(!info.oneWay) {
            globals.all_street_segments[segment].arrows_to_draw.clear();
        }
    }
}

void redrawStreetComponents(ezgl::renderer *g, const std::vector<StreetSegmentIdx>& route) {
    for(auto segment:route){
       auto i = globals.all_street_segments[segment];
       //draw arrows
            g->set_line_width(i.arrow_width);
            g->set_color(i.arrow_colour);
            for (uint j = 0; j < i.arrows_to_draw.size(); ++j) {
                g->draw_line(i.arrows_to_draw[j].first, i.arrows_to_draw[j].second);
            }

        //draw street names
        g->set_text_rotation(i.text_rotation);
        g->format_font("", ezgl::font_slant::normal, ezgl::font_weight::bold, 12);
        if (!globals.dark_mode){
            g->set_color(i.text_colour);
        }
        else{
            g->set_color(i.dark_text_colour);
        }
        for (uint j = 0; j < i.text_to_draw.size(); ++j) {
            g->draw_text(i.text_to_draw[j].loc, i.text_to_draw[j].label, i.text_to_draw[j].length_x, i.text_to_draw[j].length_y);
        }
    }
}

std::vector<std::string> findDirections(const std::vector<StreetSegmentIdx >& route){
    //loop through all segments and determine if it is left / right /straight turns
    std::vector<std::string> directions;
    directions.resize(route.size());

    int i;
    for(i = 0; i < route.size()-1; i++){
        Directions direc = findAngleSegments(route[i],route[i+1]);

        switch(direc){
            case Directions::STRAIGHT:
                directions[i] ="Go straight at: ";
            break;

            case Directions::LEFT:
                directions[i] = "Turn left at: ";
                break;

            case Directions::RIGHT:
                directions[i] ="Turn right at:  ";
                break;

            case Directions::U_turn:
                directions[i]="Make U turn at:  ";
                break;

            default:
                directions[i] ="Continue straight towards: ";
        }
    }
    return directions;
}

Directions findAngleSegments(StreetSegmentIdx from, StreetSegmentIdx to){
    //calculate the angle between the two segment and the intermediate point, and then substract
    double pi = std::acos(-1);
    street_segment_info info_from = globals.all_street_segments[from];
    street_segment_info info_to = globals.all_street_segments[to];
    ezgl::point2d src_pos, intermediate,dst_pos;
    bool from_curved = true;
    bool to_curved = true;
    if(info_from.num_curve_point==0){
        from_curved = false;
    }
    if(info_to.num_curve_point ==0){
        to_curved=false;
    }

    //check which way is the two street segment connected
    if(globals.all_intersections[info_from.to].index == globals.all_intersections[info_to.from].index){
        // from -> to & from ->to
        if(from_curved){
            //take the last curve point
//            src_pos = from_curves->at(from_curves->size()-1);
            src_pos = info_from.lines_to_draw[info_from.lines_to_draw.size()-1].first;
        }
        else {
            src_pos = globals.all_intersections[info_from.from].position;
        }
        if(to_curved){
            //take the first curve point
            dst_pos = info_to.lines_to_draw[0].second;
        }
        else{
            dst_pos = globals.all_intersections[info_to.to].position;
        }
         intermediate = globals.all_intersections[info_from.to].position;
    }
    else if(globals.all_intersections[info_from.from].index == globals.all_intersections[info_to.from].index){
        // to -> from & from -> to
        if(from_curved){
            //take the first curve point
            src_pos = info_from.lines_to_draw[0].second;
        }
        else{
            src_pos= globals.all_intersections[info_from.to].position;
        }
        if(to_curved){
            //take the first curve point
            dst_pos = info_to.lines_to_draw[0].second;
        }
        else{
            dst_pos = globals.all_intersections[info_to.to].position;
        }
        intermediate = globals.all_intersections[info_from.from].position;
    }
    else if(globals.all_intersections[info_from.to].index == globals.all_intersections[info_to.to].index){
        //from -> to & to -> from
        if(from_curved){
            //take the last curve point
            src_pos = info_from.lines_to_draw[info_from.lines_to_draw.size()-1].first;
        }
        else {
            src_pos = globals.all_intersections[info_from.from].position;
        }
        if(to_curved){
            //take the last curve point
            dst_pos = info_to.lines_to_draw[info_to.lines_to_draw.size()-1].first;
        }
        else{
            dst_pos = globals.all_intersections[info_to.from].position;
        }
        intermediate = globals.all_intersections[info_from.to].position;
    }
    else{
        //to -> from & to -> from
        if(from_curved){
            //take the first curve point
            src_pos = info_from.lines_to_draw[0].second;
        }
        else{
            src_pos= globals.all_intersections[info_from.to].position;
        }
        if(to_curved){
            //take the last curve point
            dst_pos = info_to.lines_to_draw[info_to.lines_to_draw.size()-1].first;
        }
        else{
            dst_pos = globals.all_intersections[info_to.from].position;
        }
        intermediate = globals.all_intersections[info_from.from].position;
    }

    double src_x = intermediate.x - src_pos.x;
    double src_y = intermediate.y - src_pos.y;
    double dst_x = dst_pos.x - intermediate.x;
    double dst_y = dst_pos.y - intermediate.y;

    //calculate using atan2 to get from -pi to pi
    double init_angle = atan2(src_y, src_x);
    double dst_angle = atan2(dst_y,dst_x);

    Directions turn_direc;

    // if the two angles have the same sign
    if(init_angle * dst_angle > 0) {
        //if init angle is positive
        if (init_angle > 0) {
            if (abs(dst_angle) > abs(init_angle)) {
                turn_direc = Directions::LEFT;
            } else if (abs(dst_angle) < abs(init_angle)) {
                turn_direc = Directions::RIGHT;
            } else {
                turn_direc = Directions::STRAIGHT;
            }
        }
            //  if init_angle is negative
        else {
            if (abs(dst_angle) > abs(init_angle)) {
                turn_direc = Directions::RIGHT;
            } else if (abs(dst_angle) < abs(init_angle)) {
                turn_direc = Directions::LEFT;
            } else {
                turn_direc = Directions::STRAIGHT;
            }
        }
    }
    //if the two angles have opposite sign
    else if(init_angle*dst_angle < 0) {
        if (init_angle > 0) {
            if (abs(dst_angle) > abs(pi - init_angle)) {
                turn_direc = Directions::LEFT;
            } else if (abs(dst_angle) < abs(pi - init_angle)) {
                turn_direc = Directions::RIGHT;
            } else {
                turn_direc = Directions::STRAIGHT;
            }
        }
            //  if init_angle is negative
        else {
            if (abs(dst_angle) > abs(pi + init_angle)) {
                turn_direc = Directions::RIGHT;
            } else if (abs(dst_angle) < abs(pi + init_angle)) {
                turn_direc = Directions::LEFT;
            } else {
                turn_direc = Directions::STRAIGHT;
            }
        }
    }
    // if one of the angle is 0
    else {
        if (init_angle > dst_angle) {
            turn_direc = Directions::RIGHT;
        } else if (init_angle < dst_angle) {
            turn_direc = Directions::LEFT;
        } else {
            turn_direc = Directions::STRAIGHT;
        }
    }

    if(abs(init_angle-dst_angle) <0.17){
        turn_direc = Directions::STRAIGHT;
    }

    return turn_direc;
}