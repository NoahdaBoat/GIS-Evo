
#include <string>
#include "StreetsDatabaseAPI.h"
#include "globals.h"
#include "ms1helpers.h"
#include "streetsegment_info.hpp"
#include <algorithm>
#include "../ezgl/graphics.hpp"
#include <string>
#include "../globals.h"

// this function sets the colour, and which zoom level it is shown at depending on which type of road it is
void set_colour_of_street(RoadType type, int idx) {

    switch (type) {
        case RoadType::motorway:
        case RoadType::motorway_link:
        case RoadType::trunk:
        case RoadType::trunk_link:

            // zoom level, line width

            // the first value in the push_back pair is the value the lod must be greater than in order to display

            globals.all_street_segments[idx].zoom_levels.push_back({-5, 2});
            globals.all_street_segments[idx].zoom_levels.push_back({3, 3});
            globals.all_street_segments[idx].zoom_levels.push_back({7, 8});

            globals.all_street_segments[idx].road_colour = ezgl::color(246, 207, 101, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(118, 163, 205, 255);

            break;
        
        case RoadType::primary:
        case RoadType::primary_link:

            globals.all_street_segments[idx].zoom_levels.push_back({2, 0});
            globals.all_street_segments[idx].zoom_levels.push_back({4, 4});
            globals.all_street_segments[idx].zoom_levels.push_back({7, 6});
            //g->set_line_width(zoom);

            globals.all_street_segments[idx].road_colour = ezgl::color(246, 207, 101, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(118, 163, 205, 255);

            break;
        
        case RoadType::secondary:
        case RoadType::secondary_link:

            globals.all_street_segments[idx].zoom_levels.push_back({4, 0});
            globals.all_street_segments[idx].zoom_levels.push_back({6, 3});
            globals.all_street_segments[idx].zoom_levels.push_back({8, 5});
            globals.all_street_segments[idx].road_colour = ezgl::color(174, 164, 164, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(113, 133, 152, 255);

            break;
        
        case RoadType::tertiary:
        case RoadType::tertiary_link:

            globals.all_street_segments[idx].zoom_levels.push_back({5, 0});
            globals.all_street_segments[idx].zoom_levels.push_back({8, 3});
            globals.all_street_segments[idx].zoom_levels.push_back({10, 5});
            globals.all_street_segments[idx].road_colour = ezgl::color(174, 164, 164, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(113, 133, 152, 255);

            break;

        case RoadType::road:

            globals.all_street_segments[idx].zoom_levels.push_back({5, 0});
            globals.all_street_segments[idx].zoom_levels.push_back({8, 3});
            globals.all_street_segments[idx].zoom_levels.push_back({10, 5});
            globals.all_street_segments[idx].road_colour = ezgl::color(0, 0, 0, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(90, 110, 129, 255);

            break;

        case RoadType::service:

            globals.all_street_segments[idx].zoom_levels.push_back({8, 0});
            globals.all_street_segments[idx].road_colour = ezgl::color(174, 164, 164, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(90, 110, 129, 255);

            break;

        case RoadType::footway:
        case RoadType::path:
        case RoadType::bridleway:
        case RoadType::trail:
        case RoadType::pedestrian:

            globals.all_street_segments[idx].zoom_levels.push_back({8, 0});
            globals.all_street_segments[idx].road_colour = ezgl::color(18, 68, 41, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(90, 110, 129, 255);

            break;

        case RoadType::cycleway:

            globals.all_street_segments[idx].zoom_levels.push_back({8, 0});
            globals.all_street_segments[idx].road_colour = ezgl::color(128, 128, 128, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(90, 110, 129, 255);

            break;

        case RoadType::residential:
        case RoadType::living_street:

            globals.all_street_segments[idx].zoom_levels.push_back({6, 0});
            globals.all_street_segments[idx].zoom_levels.push_back({8, 3});
            globals.all_street_segments[idx].zoom_levels.push_back({10, 5});
            globals.all_street_segments[idx].road_colour = ezgl::color(192, 192, 192, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(113, 133, 152, 255);

            break;

        default:
            globals.all_street_segments[idx].zoom_levels.push_back({8, 0});
            globals.all_street_segments[idx].road_colour = ezgl::color(174, 164, 164, 255);
            globals.all_street_segments[idx].dark_road_colour = ezgl::color(90, 110, 129, 255);

            break;
    }
}

void draw_arrows(int idx, ezgl::point2d from, ezgl::point2d to) {
    double arrow_length = 10;
    double arrowhead_length = arrow_length / 2;
    double spacing = 2 * arrow_length;

    // arrow starts at point from
    // calculate the three other points of the arrow
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double length = sqrt(dx * dx + dy * dy);
    if (length == 0 || length < spacing + arrow_length){
        return;
    }
    double unit_dx = dx / length;
    double unit_dy = dy / length;

    from.x += unit_dx * arrow_length * 0.5;
    from.y += unit_dy * arrow_length * 0.5;

    double remaining_length = length - 0.5 * arrow_length;
    while (remaining_length >= 1.5 * arrow_length){

        ezgl::point2d arrow_shaft_end(
                from.x + unit_dx * arrow_length,
                from.y + unit_dy * arrow_length
        );

        double angle = atan2(unit_dy, unit_dx);
        double arrow_angle = M_PI / 8; // 22.5 degrees

        ezgl::point2d arrow_left_end(
                arrow_shaft_end.x - arrowhead_length * cos(angle - arrow_angle),
                arrow_shaft_end.y - arrowhead_length * sin(angle - arrow_angle)
        );
        ezgl::point2d arrow_right_end(
                arrow_shaft_end.x - arrowhead_length * cos(angle + arrow_angle),
                arrow_shaft_end.y - arrowhead_length * sin(angle + arrow_angle)
        );

        globals.all_street_segments[idx].arrows_to_draw.push_back({from, arrow_shaft_end});
        globals.all_street_segments[idx].arrows_to_draw.push_back({arrow_shaft_end, arrow_left_end});
        globals.all_street_segments[idx].arrows_to_draw.push_back({arrow_shaft_end, arrow_right_end});

        from.x += unit_dx * (spacing + arrow_length);
        from.y += unit_dy * (spacing + arrow_length);
        remaining_length -= (spacing + arrow_length);
    }
}

double calculate_angle(double from_pos_x, double from_pos_y, double to_pos_x, double to_pos_y){

    if (from_pos_x < to_pos_x){
        std::swap(from_pos_x, to_pos_x);
        std::swap(from_pos_y, to_pos_y);
    }

    double x_diff = from_pos_x - to_pos_x;
    double y_diff = from_pos_y - to_pos_y;

    if (x_diff == 0 && y_diff == 0){
        return 0;
    }
    else{
        double angle = atan2(y_diff, x_diff) * (180.0/M_PI);

        if (angle > 90 && angle < 270){
            angle -= 180;
        }
        return angle;
    }
}

void compute_streets_info() {

    globals.all_street_segments.resize(getNumStreetSegments());

    for (uint i = 0; i < getNumStreetSegments(); ++i) {
        StreetSegmentInfo info = getStreetSegmentInfo(i);

        globals.all_street_segments[i].arrow_width = 1;
        globals.all_street_segments[i].arrow_colour = ezgl::BLACK;
        globals.all_street_segments[i].arrow_zoom_dep = 9;
        globals.all_street_segments[i].text_colour = ezgl::BLACK;
        globals.all_street_segments[i].dark_text_colour = ezgl::WHITE;
        globals.all_street_segments[i].type = globals.ss_road_type[i];
        globals.all_street_segments[i].street = info.streetID;
        globals.all_street_segments[i].street_name = getStreetName(info.streetID);
        globals.all_street_segments[i].inter_from = getIntersectionName(info.from);
        globals.all_street_segments[i].inter_to = getIntersectionName(info.to);
        globals.all_street_segments[i].from = info.from;
        globals.all_street_segments[i].to = info.to;
        globals.all_street_segments[i].num_curve_point = info.numCurvePoints;



        set_colour_of_street(globals.ss_road_type[i], i);
        // type
        // road_color

        // only used for drawing the A* algorithm
        globals.all_street_segments[i].index = i;
        globals.all_street_segments[i].to = info.to;
        globals.all_street_segments[i].from = info.from;
        globals.all_street_segments[i].oneWay = info.oneWay;
        globals.all_street_segments[i].speedLimit = info.speedLimit;

        OSMID current_street_id = info.wayOSMID;

        globals.all_street_segments[i].id = current_street_id;

        LatLon from_pos = getIntersectionPosition(info.from);
        LatLon to_pos = getIntersectionPosition(info.to);
        double from_pos_x, from_pos_y, to_pos_x, to_pos_y;
        convertLatLonToXY(from_pos, from_pos_x, from_pos_y, globals.map_lat_avg);
        convertLatLonToXY(to_pos, to_pos_x, to_pos_y, globals.map_lat_avg);
        double pos_avg_x = (from_pos_x+to_pos_x)/2;
        double pos_avg_y = (from_pos_y+to_pos_y)/2;
        globals.all_street_segments[i].x_avg = pos_avg_x;
        globals.all_street_segments[i].y_avg = pos_avg_y;

        // initialize max and min position of street segment
        double max_x = std::max(from_pos_x, to_pos_x);
        double max_y = std::max(from_pos_y, to_pos_y);
        double min_x = std::min(from_pos_x, to_pos_x);
        double min_y = std::min(from_pos_y,to_pos_y);

        if (info.numCurvePoints != 0) {
            LatLon first_curve_point = getStreetSegmentCurvePoint(0, i);
            LatLon last_curve_point = getStreetSegmentCurvePoint(info.numCurvePoints - 1, i);
            double first_x, first_y, last_x, last_y;
            convertLatLonToXY(first_curve_point, first_x, first_y, globals.map_lat_avg);
            convertLatLonToXY(last_curve_point, last_x, last_y, globals.map_lat_avg);

            //compare with intersection to and from
            max_x =std::max(max_x, first_x);
            max_y =std::max(max_y, first_y);
            min_x = std::min(min_x, first_x);
            min_y = std::min(min_y, first_y);

            globals.all_street_segments[i].lines_to_draw.push_back({{from_pos_x, from_pos_y}, {first_x, first_y}});

            if (info.oneWay) {
                draw_arrows(i, {from_pos_x, from_pos_y}, {first_x, first_y});
            }

            for (int j = 0; j < info.numCurvePoints - 1; j++) {
                LatLon front_curve_point = getStreetSegmentCurvePoint(j, i);
                LatLon back_curve_point = getStreetSegmentCurvePoint(j + 1, i);
                double front_x, front_y, back_x, back_y;
                convertLatLonToXY(front_curve_point, front_x, front_y, globals.map_lat_avg);
                convertLatLonToXY(back_curve_point, back_x, back_y, globals.map_lat_avg);

                //compare the position of all curve points to get max and min positions
                max_x =std::max(max_x, back_x);
                max_y =std::max(max_y, back_y);
                min_x = std::min(min_x, back_x);
                min_y = std::min(min_y, back_y);

                globals.all_street_segments[i].lines_to_draw.push_back({{front_x, front_y}, {back_x, back_y}});

                if (info.oneWay) {
                    draw_arrows(i, {front_x, front_y}, {back_x, back_y});
                }
            }

            globals.all_street_segments[i].lines_to_draw.push_back({{last_x, last_y}, {to_pos_x, to_pos_y}});

            if (info.oneWay) {
                draw_arrows(i, {last_x, last_y}, {to_pos_x, to_pos_y});
            }
        }
        else {
            globals.all_street_segments[i].lines_to_draw.push_back({{from_pos_x, from_pos_y}, {to_pos_x, to_pos_y}});

            if (info.oneWay) {
                draw_arrows(i, {from_pos_x, from_pos_y}, {to_pos_x, to_pos_y});
            }
        }


        globals.all_street_segments[i].max_pos = {max_x,max_y};
        globals.all_street_segments[i].min_pos = {min_x,min_y};

        // draw street names
        // calculates angle of street name and draws street names
        std::string street_name = getStreetName(info.streetID);
        double segment_length = globals.vec_segmentdis[i].segment_length;
        double name_pos_x = (from_pos_x + to_pos_x) / 2;
        double name_pos_y = (from_pos_y + to_pos_y) / 2;

        if (street_name == "<unknown>") {
            continue;
        }
        globals.all_street_segments[i].text_rotation = calculate_angle(from_pos_x, from_pos_y, to_pos_x, to_pos_y);

        text_prop text;
        text.label = street_name;
        text.loc = {name_pos_x, name_pos_y};
        text.length_x = segment_length;
        text.length_y = 100;
        globals.all_street_segments[i].text_to_draw.push_back(text);

    }
}
