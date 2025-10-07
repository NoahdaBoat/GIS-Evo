#include "m2_way_helpers.hpp"
#include <vector>
#include <algorithm>
#include <execution>
#include "../ms1helpers.h"
#include "LatLon.h"
#include "OSMDatabaseAPI.h"
#include <unordered_map>
#include <sstream>
#include "../ezgl/point.hpp"
#include "../Coordinates_Converstions/coords_conversions.hpp"
#include "../sort_streetseg/streetsegment_info.hpp"
#include "globals.h"
std::unordered_map<std::string, RoadType> const str_to_enum = { {"primary", RoadType::primary},
                                                                {"residential", RoadType::residential},
                                                                {"tertiary", RoadType::tertiary},
                                                                {"service", RoadType::service},
                                                                {"motorway", RoadType::motorway},
                                                                {"motorway_link", RoadType::motorway_link},
                                                                {"trunk", RoadType::trunk},
                                                                {"secondary", RoadType::secondary},
                                                                {"trunk_link", RoadType::trunk_link},
                                                                {"primary_link", RoadType::primary_link},
                                                                {"secondary_link", RoadType::secondary_link},
                                                                {"living_street", RoadType::living_street},
                                                                {"footway", RoadType::footway},
                                                                {"pedestrian", RoadType::pedestrian},
                                                                {"unclassified", RoadType::unclassified},
                                                                {"cycleway", RoadType::cycleway},
                                                                {"path", RoadType::path},
                                                                {"tertiary_link", RoadType::tertiary_link},
                                                                {"bridleway", RoadType::bridleway},
                                                                {"trail", RoadType::trail},
                                                                {"road", RoadType::road} };

std::unordered_map<std::string, way_enums> const str_to_way_class { {"embedded_rails:lanes", way_enums::tram},
                                              {"embedded_rails", way_enums::tram},
                                              };
std::vector<subway_info> subway_lines;

/*
 * Creates a vector of structs way_info, used to draw all ways in m2.cpp
 * Runtime -> worst case N*log(n) from the sorting, used to put in largest to smallest order
 * for the proper drawing setup
 */
std::vector<way_info> create_vector_of_ways(std::unordered_map<OSMID, feature_data*>& id_to_way) {

    // need to go through all ways, get the osmids, convert to nodes, get
    // the latlon, convert to point2d, store in vector
    std::vector<OSMID> way_nodes;
    std::vector<way_info> all_ways_info;
    //all_ways_info.resize(getNumberOfWays());
    for (int i = 0; i < getNumberOfWays(); ++i) {
        const OSMWay *current_way = getWayByIndex(i);
        way_info info;
        info.way_use = way_enums::notrail;
        if (!current_way->isClosed()) {
            info.is_closed = current_way->isClosed();
            info.way_id = current_way->id();
//            auto length_search = globals.way_distance.find(current_way->id());
//            info.length = length_search->second;
            auto feature_info_search = id_to_way.find(current_way->id());
            if (feature_info_search != id_to_way.end()) {
                info.way_name = feature_info_search->second->feature_name;
                info.way_type = feature_info_search->second->type;
            } else {
                info.way_name = "Unknown";
                info.way_type = FeatureType::UNKNOWN;
            }
            for (uint pairidx = 0; pairidx < getTagCount(current_way); ++pairidx) {
                std::pair<std::string, std::string> tag_pair = getTagPair(current_way, pairidx);
                if (tag_pair.first == "highway") {
                    auto find_second = str_to_enum.find(tag_pair.second);
                    if (find_second != str_to_enum.end()) {
                        info.way_road_type = find_second->second;
                    } else {
                        info.way_road_type = RoadType::other;
                    }
                }
            }
            for (uint j = 0; j < getTagCount(current_way); ++j) {
                std::pair<std::string, std::string> tag_pair = getTagPair(current_way, j);
                std::string first;

                if (tag_pair.first == "embedded_rails" || tag_pair.first == "embedded_rails:lanes") {
                    info.way_use = way_enums::tram;
                }
                else if (tag_pair.first == "railway") {
                    if (tag_pair.second == "tram") {
                        info.way_use = way_enums::tram;
                    } else if (tag_pair.second == "rail") {
                        info.way_use = way_enums::train;
                    } else if (tag_pair.second == "monorail") {
                        info.way_use = way_enums::monorail;
                    } else if (tag_pair.second == "subway") {
                        info.way_use = way_enums::subway;
                    } else {
                        info.way_use = way_enums::otherrail;
                    }
                }
                else if (tag_pair.first == "tracks") {
                    info.way_use = way_enums::tracks;
                }

            }
            way_nodes = getWayMembers(current_way);
            for (int j = 0; j < way_nodes.size(); ++j) {
                auto search = globals.node_to_id.find(way_nodes[j]);
                const OSMNode *current_node = search->second;
                LatLon node_position = current_node->coords();
                double x_pos = lon_to_x(node_position.longitude());
                double y_pos = lat_to_y(node_position.latitude());
                ezgl::point2d current_point2d = ezgl::point2d(x_pos, y_pos);
                info.way_points2d.push_back(current_point2d);
            }
        }
        all_ways_info.push_back(info);
    }
    return all_ways_info;
}

//
//bool compare_structs(const way_info& structure1, const way_info& structure2) {
//    return structure1.length > structure2.length;
//}

std::unordered_map<OSMID, int> map_way_to_idx() {
    std::unordered_map<OSMID, int> umap_to_return;

    for (unsigned int i = 0; i < getNumberOfWays(); ++i) {
        const OSMWay *curr_way = getWayByIndex(i);
        umap_to_return.insert({curr_way->id(), i});
    }

    return umap_to_return;
}

void assign_type_to_way() {

    globals.ss_road_type.resize(getNumStreetSegments());
    for (uint i = 0; i < getNumStreetSegments(); ++i) {
        StreetSegmentInfo info = getStreetSegmentInfo(i);
        auto search = globals.id_to_way.find(info.wayOSMID);
        if (search != globals.id_to_way.end()) {
            const OSMWay *current_way = search->second;
            for (uint j = 0; j < getTagCount(current_way); ++j) {
                std::pair<std::string, std::string> tag_pair = getTagPair(current_way, j);
                if (tag_pair.first == "highway") {
                    auto find_enum = str_to_enum.find(tag_pair.second);
                    if (find_enum != str_to_enum.end()) {
                        globals.ss_road_type[i] = find_enum->second;
                    }
                    else {
                        globals.ss_road_type[i] = RoadType::other;
                    }
                }
            }
        }
    }
}

void sortSubwayLines() {
    //loop through all the relations to find subway lines
    for(unsigned index_relate = 0; index_relate < getNumberOfRelations(); index_relate++){
        const OSMRelation* current_relation = getRelationByIndex(index_relate);
        for(unsigned tag = 0; tag < getTagCount(current_relation);tag++){
            std::pair<std::string,std::string> tag_pair = getTagPair(current_relation,tag);
            if(tag_pair.first =="route" && tag_pair.second =="subway"){
                //current relation is a subway
                subway_info subway_relation;
                subway_relation.members = getRelationMembers(current_relation);
                subway_relation.relation_roles= getRelationMemberRoles(current_relation);
                //loop through all the members of subway line to find the way:
                for(int member_idx = 0; member_idx < subway_relation.members.size();member_idx++){
                    TypedOSMID osmId = subway_relation.members[member_idx];
                    if(osmId.type()==TypedOSMID::Way) {
                        std::vector<ezgl::point2d> a_way;
                        //Do not need to draw the platform
                        if (subway_relation.relation_roles[member_idx] != "platform") {
                            const OSMWay *way = globals.id_to_way[osmId];
                            const std::vector<OSMID> &way_nodes = getWayMembers(way);
                            //loop through the nodes in the ways
                            for (const auto node: way_nodes) {
                                const OSMNode *node_ptr = globals.node_to_id[node];
                                ezgl::point2d loc = latlonTopoint(getNodeCoords(node_ptr));
                                a_way.push_back(loc);
                            }
                            subway_relation.subway_way.push_back(a_way);
                        }
                    }
                }

                subway_relation.type = TypedOSMID::Relation;
                for(unsigned i = 0; i< getTagCount(current_relation); i++){
                    std::pair<std::string,std::string> subway_pair = getTagPair(current_relation,i);
                    if(subway_pair.first == "colour"){
                        subway_relation.colour = subway_pair.second;
                    }
                    else if(subway_pair.first =="name"){
                        subway_relation.name = subway_pair.second;
                    }
                }
                subway_relation.draw_colour = stringToRgb(subway_relation.colour);
                subway_lines.push_back(subway_relation);
                break;
            }
        }
    }
}

ezgl::color stringToRgb(std::string& colour_str){
    int red;
    int blue;
    int green;
    if(colour_str[0] == '#'){
        colour_str.erase(0,1);
        //converts string of RGB to color
        std::istringstream colour_iss (colour_str);
        int rgb;
        colour_iss>>std::hex>>rgb;
        red = (rgb >> 16) & 0xFF;
        green = (rgb >> 8) & 0xFF;
        blue = rgb & 0xFF;
    }
    //for toronto, color of subway line is given as either yellow, green or purple
    else if(colour_str == "yellow"){
        red = 0xFF;
        green = 0x79;
        blue = 0x00;
    }
    else if(colour_str == "green"){
        red = 0x00;
        green = 0x92;
        blue = 0x3F;
    }
    else if(colour_str == "purple"){
        red = 0x80;
        green = 0x27;
        blue = 0x6C;
    }
    else{
        red = 0xCE;
        green = 0x8E;
        blue = 0x00;
    }
    ezgl::color colour_rgb(red,green, blue);
    return colour_rgb;
}

