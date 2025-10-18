
#include <gtk/gtk.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <utility>
#include "StreetsDatabaseAPI.h"
#include "streetsegment_info.hpp"
#include "../gtk4_types.hpp"
#include "../binary_loader/binary_database.hpp"
#include "../Coordinates_Converstions/coords_conversions.hpp"

// Local static storage for street segments (replaces all_street_segments)
static std::vector<street_segment_info> all_street_segments;

// this function sets the colour, and which zoom level it is shown at depending on which type of road it is
void set_colour_of_street(RoadType type, int idx) {

    switch (type) {
        case RoadType::motorway:
        case RoadType::motorway_link:
        case RoadType::trunk:
        case RoadType::trunk_link:

            // zoom level, line width

            // the first value in the push_back pair is the value the lod must be greater than in order to display

            all_street_segments[idx].zoom_levels.push_back({-5, 2});
            all_street_segments[idx].zoom_levels.push_back({3, 3});
            all_street_segments[idx].zoom_levels.push_back({7, 8});

            all_street_segments[idx].road_colour = {246/255.0, 207/255.0, 101/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {118/255.0, 163/255.0, 205/255.0, 1.0};

            break;
        
        case RoadType::primary:
        case RoadType::primary_link:

            all_street_segments[idx].zoom_levels.push_back({2, 0});
            all_street_segments[idx].zoom_levels.push_back({4, 4});
            all_street_segments[idx].zoom_levels.push_back({7, 6});
            //g->set_line_width(zoom);

            all_street_segments[idx].road_colour = {246/255.0, 207/255.0, 101/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {118/255.0, 163/255.0, 205/255.0, 1.0};

            break;
        
        case RoadType::secondary:
        case RoadType::secondary_link:

            all_street_segments[idx].zoom_levels.push_back({4, 0});
            all_street_segments[idx].zoom_levels.push_back({6, 3});
            all_street_segments[idx].zoom_levels.push_back({8, 5});
            all_street_segments[idx].road_colour = {174/255.0, 164/255.0, 164/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {113/255.0, 133/255.0, 152/255.0, 1.0};

            break;
        
        case RoadType::tertiary:
        case RoadType::tertiary_link:

            all_street_segments[idx].zoom_levels.push_back({5, 0});
            all_street_segments[idx].zoom_levels.push_back({8, 3});
            all_street_segments[idx].zoom_levels.push_back({10, 5});
            all_street_segments[idx].road_colour = {174/255.0, 164/255.0, 164/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {113/255.0, 133/255.0, 152/255.0, 1.0};

            break;

        case RoadType::road:

            all_street_segments[idx].zoom_levels.push_back({5, 0});
            all_street_segments[idx].zoom_levels.push_back({8, 3});
            all_street_segments[idx].zoom_levels.push_back({10, 5});
            all_street_segments[idx].road_colour = {0.0, 0.0, 0.0, 1.0};
            all_street_segments[idx].dark_road_colour = {90/255.0, 110/255.0, 129/255.0, 1.0};

            break;

        case RoadType::service:

            all_street_segments[idx].zoom_levels.push_back({8, 0});
            all_street_segments[idx].road_colour = {174/255.0, 164/255.0, 164/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {90/255.0, 110/255.0, 129/255.0, 1.0};

            break;

        case RoadType::footway:
        case RoadType::path:
        case RoadType::bridleway:
        case RoadType::trail:
        case RoadType::pedestrian:

            all_street_segments[idx].zoom_levels.push_back({8, 0});
            all_street_segments[idx].road_colour = {18/255.0, 68/255.0, 41/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {90/255.0, 110/255.0, 129/255.0, 1.0};

            break;

        case RoadType::cycleway:

            all_street_segments[idx].zoom_levels.push_back({8, 0});
            all_street_segments[idx].road_colour = {128/255.0, 128/255.0, 128/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {90/255.0, 110/255.0, 129/255.0, 1.0};

            break;

        case RoadType::residential:
        case RoadType::living_street:

            all_street_segments[idx].zoom_levels.push_back({6, 0});
            all_street_segments[idx].zoom_levels.push_back({8, 3});
            all_street_segments[idx].zoom_levels.push_back({10, 5});
            all_street_segments[idx].road_colour = {192/255.0, 192/255.0, 192/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {113/255.0, 133/255.0, 152/255.0, 1.0};

            break;

        default:
            all_street_segments[idx].zoom_levels.push_back({8, 0});
            all_street_segments[idx].road_colour = {174/255.0, 164/255.0, 164/255.0, 1.0};
            all_street_segments[idx].dark_road_colour = {90/255.0, 110/255.0, 129/255.0, 1.0};

            break;
    }
}

void draw_arrows(int idx, Point2D from, Point2D to) {
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

        Point2D arrow_shaft_end(
                from.x + unit_dx * arrow_length,
                from.y + unit_dy * arrow_length
        );

        double angle = atan2(unit_dy, unit_dx);
        double arrow_angle = M_PI / 8; // 22.5 degrees

        Point2D arrow_left_end(
                arrow_shaft_end.x - arrowhead_length * cos(angle - arrow_angle),
                arrow_shaft_end.y - arrowhead_length * sin(angle - arrow_angle)
        );
        Point2D arrow_right_end(
                arrow_shaft_end.x - arrowhead_length * cos(angle + arrow_angle),
                arrow_shaft_end.y - arrowhead_length * sin(angle + arrow_angle)
        );

        all_street_segments[idx].arrows_to_draw.push_back({from, arrow_shaft_end});
        all_street_segments[idx].arrows_to_draw.push_back({arrow_shaft_end, arrow_left_end});
        all_street_segments[idx].arrows_to_draw.push_back({arrow_shaft_end, arrow_right_end});

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

    all_street_segments.resize(getNumStreetSegments());

    for (uint i = 0; i < getNumStreetSegments(); ++i) {
        StreetSegmentInfo info = getStreetSegmentInfo(i);

        all_street_segments[i].arrow_width = 1;
        all_street_segments[i].arrow_colour = {0.0f, 0.0f, 0.0f, 1.0f}; // BLACK
        all_street_segments[i].arrow_zoom_dep = 9;
        all_street_segments[i].text_colour = {0.0f, 0.0f, 0.0f, 1.0f}; // BLACK
        all_street_segments[i].dark_text_colour = {1.0f, 1.0f, 1.0f, 1.0f}; // WHITE
        all_street_segments[i].type = static_cast<RoadType>(gisevo::BinaryDatabase::instance().get_segment_by_index(i).category);
        all_street_segments[i].street = info.streetID;
        all_street_segments[i].street_name = getStreetName(info.streetID);
        all_street_segments[i].inter_from = getIntersectionName(info.from);
        all_street_segments[i].inter_to = getIntersectionName(info.to);
        all_street_segments[i].from = info.from;
        all_street_segments[i].to = info.to;
        all_street_segments[i].num_curve_point = info.numCurvePoints;



        set_colour_of_street(static_cast<RoadType>(all_street_segments[i].type), i);
        // type
        // road_color

        // only used for drawing the A* algorithm
        all_street_segments[i].index = i;
        all_street_segments[i].to = info.to;
        all_street_segments[i].from = info.from;
        all_street_segments[i].oneWay = info.oneWay;
        all_street_segments[i].speedLimit = info.speedLimit;

        OSMID current_street_id = info.wayOSMID;

        all_street_segments[i].id = current_street_id;

        LatLon from_pos = getIntersectionPosition(info.from);
        LatLon to_pos = getIntersectionPosition(info.to);
        Point2D from_point = latlonTopoint(from_pos, gisevo::BinaryDatabase::instance().get_avg_lat_rad());
        Point2D to_point = latlonTopoint(to_pos, gisevo::BinaryDatabase::instance().get_avg_lat_rad());
        double from_pos_x = from_point.x, from_pos_y = from_point.y;
        double to_pos_x = to_point.x, to_pos_y = to_point.y;
        double pos_avg_x = (from_pos_x+to_pos_x)/2;
        double pos_avg_y = (from_pos_y+to_pos_y)/2;
        all_street_segments[i].x_avg = pos_avg_x;
        all_street_segments[i].y_avg = pos_avg_y;

        // initialize max and min position of street segment
        double max_x = std::max(from_pos_x, to_pos_x);
        double max_y = std::max(from_pos_y, to_pos_y);
        double min_x = std::min(from_pos_x, to_pos_x);
        double min_y = std::min(from_pos_y,to_pos_y);

        if (info.numCurvePoints != 0) {
            LatLon first_curve_point = getStreetSegmentCurvePoint(0, i);
            LatLon last_curve_point = getStreetSegmentCurvePoint(info.numCurvePoints - 1, i);
            Point2D first_point = latlonTopoint(first_curve_point, gisevo::BinaryDatabase::instance().get_avg_lat_rad());
            Point2D last_point = latlonTopoint(last_curve_point, gisevo::BinaryDatabase::instance().get_avg_lat_rad());
            double first_x = first_point.x, first_y = first_point.y;
            double last_x = last_point.x, last_y = last_point.y;

            //compare with intersection to and from
            max_x =std::max(max_x, first_x);
            max_y =std::max(max_y, first_y);
            min_x = std::min(min_x, first_x);
            min_y = std::min(min_y, first_y);

            all_street_segments[i].lines_to_draw.push_back({{from_pos_x, from_pos_y}, {first_x, first_y}});

            if (info.oneWay) {
                draw_arrows(i, {from_pos_x, from_pos_y}, {first_x, first_y});
            }

            for (size_t j = 0; j < static_cast<size_t>(info.numCurvePoints) - 1; j++) {
                LatLon front_curve_point = getStreetSegmentCurvePoint(j, i);
                LatLon back_curve_point = getStreetSegmentCurvePoint(j + 1, i);
                Point2D front_point = latlonTopoint(front_curve_point, gisevo::BinaryDatabase::instance().get_avg_lat_rad());
                Point2D back_point = latlonTopoint(back_curve_point, gisevo::BinaryDatabase::instance().get_avg_lat_rad());
                double front_x = front_point.x, front_y = front_point.y;
                double back_x = back_point.x, back_y = back_point.y;

                //compare the position of all curve points to get max and min positions
                max_x =std::max(max_x, back_x);
                max_y =std::max(max_y, back_y);
                min_x = std::min(min_x, back_x);
                min_y = std::min(min_y, back_y);

                all_street_segments[i].lines_to_draw.push_back({{front_x, front_y}, {back_x, back_y}});

                if (info.oneWay) {
                    draw_arrows(i, {front_x, front_y}, {back_x, back_y});
                }
            }

            all_street_segments[i].lines_to_draw.push_back({{last_x, last_y}, {to_pos_x, to_pos_y}});

            if (info.oneWay) {
                draw_arrows(i, {last_x, last_y}, {to_pos_x, to_pos_y});
            }
        }
        else {
            all_street_segments[i].lines_to_draw.push_back({{from_pos_x, from_pos_y}, {to_pos_x, to_pos_y}});

            if (info.oneWay) {
                draw_arrows(i, {from_pos_x, from_pos_y}, {to_pos_x, to_pos_y});
            }
        }


        all_street_segments[i].max_pos = {max_x,max_y};
        all_street_segments[i].min_pos = {min_x,min_y};

        // draw street names
        // calculates angle of street name and draws street names
        std::string street_name = getStreetName(info.streetID);
        double segment_length = sqrt((to_pos_x - from_pos_x) * (to_pos_x - from_pos_x) + (to_pos_y - from_pos_y) * (to_pos_y - from_pos_y));
        double name_pos_x = (from_pos_x + to_pos_x) / 2;
        double name_pos_y = (from_pos_y + to_pos_y) / 2;

        if (street_name.compare("<unknown>") == 0) {
            continue;
        }
        all_street_segments[i].text_rotation = calculate_angle(from_pos_x, from_pos_y, to_pos_x, to_pos_y);

        text_prop text;
        text.label = street_name;
        text.loc = {name_pos_x, name_pos_y};
        text.length_x = segment_length;
        text.length_y = 100;
        all_street_segments[i].text_to_draw.push_back(text);

    }
}

// Access function for street segments (replaces globals.all_street_segments)
const std::vector<street_segment_info>& get_all_street_segments() {
    return all_street_segments;
}
