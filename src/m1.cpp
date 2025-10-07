/* 
 * Copyright 2024 University of Toronto
 *
 * Permission is hereby granted, to use this software and associated 
 * documentation files (the "Software") in course work at the University 
 * of Toronto, or for personal use. Other uses are prohibited, in 
 * particular the distribution of the Software either publicly or to third 
 * parties.
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <iostream>
#include <cmath>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <thread>
#include "m1.h"
#include "Coordinates_Converstions/coords_conversions.hpp"
#include "StreetsDatabaseAPI.h"
#include "OSMDatabaseAPI.h"
#include "ms1helpers.h"
#include "ms2helpers.hpp"
#include "../POI/POI_helpers.hpp"
#include "../POI/POI_setup.hpp"
#include "Coordinates_Converstions/coords_conversions.hpp"
#include "foursquareapi/foursquarefunctions.hpp"
#include "intersection_setup.hpp"
#include "streetsegment_info.hpp"
#include "Intersections/intersection_setup.hpp"
#include <chrono>

//#define NOT_TESTING

// global variables contained within this class/object
Global_Var globals;
//std::unordered_map<uint, std::vector<feature_info>> spatial_hash;
std::vector<std::vector<feature_info>> spatial_hash;
std::vector<feature_info> always_draw;

// Loads a map streets.bin and the corresponding osm.bin file 
// Returns true if successfull and false if error occured when loading map 
bool loadMap(std::string map_streets_database_filename) {
    bool load_successful = false;
    // bool load_layer2 = loadStreetsDatabaseBIN(map_streets_database_filename);
    // indicates whether the map has loaded
    globals.current_map_open = map_streets_database_filename;

    auto isMapLoaded = globals.loadedMap.find(map_streets_database_filename);
    // map not found in DB or it's false
    if (isMapLoaded == globals.loadedMap.end() || !isMapLoaded->second) {
        load_successful = loadStreetsDatabaseBIN(map_streets_database_filename);
        if (!load_successful) {
            return false;
        }
        else {
            globals.loadedMap.insert_or_assign(map_streets_database_filename, load_successful);
            std::string baseMapName = map_streets_database_filename;
            replaceString(baseMapName, "streets", "osm");
            loadOSMDatabaseBIN(baseMapName);
        }
    }
    else {
        // if the map was already loaded, no point to reload all data
        return true;
    }
    globals.map_lat_avg = find_map_bounds();

    globals.map_lat_avg = find_map_bounds();


    //writes to intersection_street_segments, adjacent_intersections
    std::thread t2(&preLoadIntersectionStreetSegment);

    // writes to node_to_id
    std::thread t3(&mapOSMIDToNode);

    // writes to id_to_way
    std::thread t4(&mapOSMIDToWay);

    // writes to id_to_relation
    std::thread t5(&mapOSMIDToRelation);

    // writes to ordered_street_name, vec_streetinfo, initilizes street_length
    std::thread t6(&loopAllStreets);

    // writes to poi_sorted
    std::thread t9(&sortPOI);

    // writes to vecPng
    std::thread t10(&load_image_files);
    //preLoadAjacentIntersections();

    t3.join();
    // reads from node_to_id, writes to way_distance
    //std::thread t7(&preLoadWayDistance);
    t6.join();

    // writes to vec_streetinfo
    std::thread t8(&loopAllStreetSegments);

    std::thread t11(&fill_intersection_info);

    std::thread t7(&sort_features);

    m2_local_id_to_feature = map_features_to_ways(m2_local_all_features_info);
    assign_type_to_way();
    auto start = std::chrono::high_resolution_clock::now();
    t2.join();
    t4.join();
    t5.join();
    m2_local_all_ways_info = create_vector_of_ways(m2_local_id_to_feature);
    t8.join();
    compute_streets_info();
    t9.join();
    t10.join();
    t11.join();
    t7.join();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);


    //fill_intersection_info();
    sort_features();
    loadMapNames();
    std::string city;
    std::string country;
    bool found = false;
    int j = 0;
    while (!found && j < globals.map_path_to_name.size()) {
        if (globals.map_path_to_name[j].first == map_streets_database_filename) {
            city = globals.map_path_to_name[j].second.city_name;
            country = globals.map_path_to_name[j].second.country_name;
            found = true;
        }
        ++j;
    }
    // turned off for testing due to an api request rate limit, uncommenting the code will re-enable the api automatically
#ifdef NOT_TESTING
    get_foursquare_data("restaurants", city, country);
    parse_foursquare_data("restaurants", city, country);
    get_foursquare_data("shops", city, country);
    parse_foursquare_data("shops", city, country);
#endif
    initSubwayStations();
    sortSubwayLines();
    //std::cout << duration.count() << std::endl;
    for(int i = 0; i <= NUM_POI_basics; i++){
        bool state = true;
        globals.draw_which_poi.push_back(state);
    }
    return load_successful;
}

// Closes the map 
void closeMap() {
    // Clean-up your map related data structures here
    auto isMapOpen = globals.loadedMap.find(globals.current_map_open);
    if (isMapOpen != globals.loadedMap.end() && isMapOpen->second) { // map in DB, and it's open
        globals.loadedMap.insert_or_assign(globals.current_map_open, false); // set the map to false so it's closed now
    }
    globals.way_distance.clear();
    globals.node_to_id.clear();
    globals.ordered_street_name.clear();
    globals.intersection_street_segments.clear();
    closeOSMDatabase();
    closeStreetDatabase();
    globals.vec_streetinfo.clear();
    globals.vec_segmentdis.clear();
    globals.adjacent_intersections.clear();
    globals.all_intersections.clear();
    globals.id_to_way.clear();
    globals.id_to_relation.clear();
    globals.poi_sorted.basic_poi.clear();
    globals.poi_sorted.entertainment_poi.clear();
    globals.poi_sorted.subordinate_poi.clear();
    globals.poi_sorted.basic_poi.clear();
    globals.poi_sorted.neglegible_poi.clear();
    globals.poi_sorted.stations_poi.clear();
    m2_local_all_ways_info.clear();
    m2_local_all_features_info.clear();
    m2_local_id_to_feature.clear();
    m2_local_way_to_idx.clear();
    m2_local_index_of_open_way.clear();
    m2_local_all_street_types.clear();
    m2_local_all_relations_vector.clear();
    closed_features.clear();
    open_features.clear();
    //searched_intersections.clear();
    current_zoom_level = 0;
    x_zoom_prev = 0;
    y_zoom_prev = 0;
    globals.city_restaurants.clear();
    globals.city_shops.clear();
    globals.ss_road_type.clear();
    globals.all_street_segments.clear();

    subway_lines.clear();
    highlighted_intersections.clear();

    for (uint i = 0; i < globals.vec_png.zoom_in.size(); ++i) {
        cairo_surface_destroy((globals.vec_png.zoom_in[i]));
    }
    for (uint i = 0; i < globals.vec_png.zoom_out.size(); ++i) {
        cairo_surface_destroy((globals.vec_png.zoom_out[i]));
    }
    globals.vec_png.zoom_in.clear();
    globals.vec_png.zoom_out.clear();
    globals.max_speed = 0;

}

// Returns the distance between two (latitude,longitude) coordinates in meters.
double findDistanceBetweenTwoPoints(LatLon point_1, LatLon point_2) {
    double lat1 = kDegreeToRadian * point_1.latitude();
    double lon1 = kDegreeToRadian * point_1.longitude();
    double lat2 = kDegreeToRadian * point_2.latitude();
    double lon2 = kDegreeToRadian * point_2.longitude();
    double x1, y1, x2, y2;
    double latavg = (lat2 + lat1)/2;
    double distance;
    // using cmath cos
    x1 = kEarthRadiusInMeters * lon1 * cos(latavg);
    y1 = kEarthRadiusInMeters * lat1;
    x2 = kEarthRadiusInMeters * lon2 * cos(latavg);
    y2 = kEarthRadiusInMeters * lat2;
    // pow does -> (y2-y1)^2
    distance = sqrt(pow((y2 - y1), 2) + pow((x2 - x1), 2));
    return distance;
}

// Returns the length of a given street in meters.
double findStreetSegmentLength(StreetSegmentIdx street_segment_id) {
    //return 0 if invalid id
    if (street_segment_id >= getNumStreetSegments()){
        return 0;
    }
    double length = globals.vec_segmentdis[street_segment_id].segment_length;
    return length;
}

// Returns the travel time to drive from one end to the other end of street segment, if traveling at speed limit
// Time in seconds. 
double findStreetSegmentTravelTime(StreetSegmentIdx street_segment_id) {
    //check for invalid input
    if (street_segment_id >= getNumStreetSegments()){
        return 0;
    }
    double travel_time = globals.vec_segmentdis[street_segment_id].travel_time;
    return travel_time;
}

// Given a src_street_segment_id and dst_street_segment_id
// if the street segments share an intersection, return the angle (in radians) between them
// if the street segments do not share an intersection, return the constant NO_ANGLE
// if street segment is not completely straight, use the piece of segment closest to intersection
double findAngleBetweenStreetSegments(StreetSegmentIdx src_street_segment_id,
                                      StreetSegmentIdx dst_street_segment_id) {

    StreetSegmentInfo src_street_segment_info = getStreetSegmentInfo(src_street_segment_id);
    StreetSegmentInfo dst_street_segment_info = getStreetSegmentInfo(dst_street_segment_id);

    IntersectionIdx src_from = src_street_segment_info.from;
    IntersectionIdx src_to = src_street_segment_info.to;
    IntersectionIdx dst_from = dst_street_segment_info.from;
    IntersectionIdx dst_to = dst_street_segment_info.to;

    // point1 is LonLat of src, point2 is LonLat of dst and point3 is LonLat of intersection
    LatLon point1, point2, point3;

    // determine which intersection of the given segments are intersecting 
    // initialize point3 to be the LatLon of intersection
    // initialize point1 to be the LatLon of closet curve point to intersection on src street 
    // initialize point1 to be the LatLon of closet curve point to intersection on dst street 
    if (src_from == dst_from){
        if (src_street_segment_info.numCurvePoints == 0){
            point1 = getIntersectionPosition(src_to);
        }
        else{
            point1 = getStreetSegmentCurvePoint(0, src_street_segment_id);
        }

        if (dst_street_segment_info.numCurvePoints == 0){
            point2 = getIntersectionPosition(dst_to);
        }
        else{
            point2 = getStreetSegmentCurvePoint(0, dst_street_segment_id);
        }
        point3 = getIntersectionPosition(src_from);
    }
    else if (src_to == dst_from){
        if (src_street_segment_info.numCurvePoints == 0){
            point1 = getIntersectionPosition(src_from);
        }
        else{
            point1 = getStreetSegmentCurvePoint(src_street_segment_info.numCurvePoints - 1, src_street_segment_id);
        }

        if (dst_street_segment_info.numCurvePoints == 0){
            point2 = getIntersectionPosition(dst_to);
        }
        else{
            point2 = getStreetSegmentCurvePoint(0, dst_street_segment_id);
        }
        point3 = getIntersectionPosition(src_to);
    }
    else if (src_from == dst_to){
        if (src_street_segment_info.numCurvePoints == 0){
            point1 = getIntersectionPosition(src_to);
        }
        else{
            point1 = getStreetSegmentCurvePoint(0, src_street_segment_id);
        }

        if (dst_street_segment_info.numCurvePoints == 0){
            point2 = getIntersectionPosition(dst_from);
        }
        else{
            point2 = getStreetSegmentCurvePoint(dst_street_segment_info.numCurvePoints - 1, dst_street_segment_id);
        }
        point3 = getIntersectionPosition(src_from);
    }
    else if (src_to == dst_to){
        if (src_street_segment_info.numCurvePoints == 0){
            point1 = getIntersectionPosition(src_from);
        }
        else{
            point1 = getStreetSegmentCurvePoint(src_street_segment_info.numCurvePoints - 1, src_street_segment_id);
        }

        if (dst_street_segment_info.numCurvePoints == 0){
            point2 = getIntersectionPosition(dst_from);
        }
        else{
            point2 = getStreetSegmentCurvePoint(dst_street_segment_info.numCurvePoints - 1, dst_street_segment_id);
        }
        point3 = getIntersectionPosition(src_to);
    }
    else{
        return NO_ANGLE;
    }

    // calculate the sides of the triangle formed by point1, point2 and point3
    double side1 = findDistanceBetweenTwoPoints(point2, point3);
    double side2 = findDistanceBetweenTwoPoints(point1, point3);
    double side3 = findDistanceBetweenTwoPoints(point1, point2);

    // calculate the angle across side3 (at point3) using cosine law 
    // limit the input of consine law to be within -1 and 1 inclusive 
    double input = (side1*side1 + side2*side2 - side3*side3) / (2 * side1 * side2);
    input = std::max(-1.0, std::min(1.0, input));
    double output = std::acos(input);
    double pi = std::acos(-1);

    return pi - output;
}

// Returns true if the two intersections are directly connected by one street segment
bool intersectionsAreDirectlyConnected(std::pair<IntersectionIdx, IntersectionIdx> intersection_ids) {

    // intersections are directly connected if they are the same intersection
    if (intersection_ids.first == intersection_ids.second){
        return true;
    }

    // retrieve the adjacent intersections of the first given intersection
    std::vector<IntersectionIdx> intersections = globals.adjacent_intersections[intersection_ids.first];

    // return true if an adjacent intersection is the second given intersection
    for (auto intersection : intersections){

        if (intersection == intersection_ids.second){
            return true;
        }
    }
    return false;
}

// Returns the geographically nearest intersection to the given position
IntersectionIdx findClosestIntersection(LatLon my_position) {

    // initialize closest intersection
    double min_distance = findDistanceBetweenTwoPoints(my_position, getIntersectionPosition(0));
    IntersectionIdx closest_intersection = 0;

    // loop through all intersections and update closest intersection if distance  
    // to my_position is less than the current closest intersection 
    int num_intersection = getNumIntersections();
    for (IntersectionIdx i = 0; i < num_intersection; i++){
        double distance = findDistanceBetweenTwoPoints(my_position, getIntersectionPosition(i));
        if (distance < min_distance){
            min_distance = distance;
            closest_intersection = i;
        }
    }
    return closest_intersection;
}

// Returns the street segments connected to the given intersection
std::vector<StreetSegmentIdx> findStreetSegmentsOfIntersection(IntersectionIdx intersection_id) {
    if (intersection_id > getNumIntersections() - 1){
        std::vector<StreetSegmentIdx> empty;
        return empty;
    }
    return globals.intersection_street_segments[intersection_id];
}

// Returns all intersections along the given street
std::vector<IntersectionIdx> findIntersectionsOfStreet(StreetIdx street_id) {
    // if the street id is invalid
    if(street_id > getNumStreets() - 1){
        std::vector<IntersectionIdx> empty;
        return empty;
    }
    return globals.vec_streetinfo[street_id].intersections;
}

// Return all IntersectionIdx at which the two given streets intersect
// could have more than one IntersectionIdx for curved streets
std::vector<IntersectionIdx> findIntersectionsOfTwoStreets(std::pair<StreetIdx, StreetIdx> street_ids) {

    // find the intersections on the two given streets
    std::vector<IntersectionIdx> intersections1 = globals.vec_streetinfo[street_ids.first].intersections;
    std::vector<IntersectionIdx> intersections2 = globals.vec_streetinfo[street_ids.second].intersections;
    std::vector<IntersectionIdx> common_intersections;

    // find the intersections common to both streets
    std::set_intersection(intersections1.begin(), intersections1.end(), intersections2.begin(), intersections2.end(), back_inserter(common_intersections));
    return common_intersections;
}

// Returns a vector of all street ids who's street name starting with the given prefix.
std::vector<StreetIdx> findStreetIdsFromPartialStreetName(std::string street_prefix) {
    std::vector<StreetIdx> found_streets;
    // remove the spaces in the given prefix and convert prefix to all lower case
    street_prefix.erase(std::remove(street_prefix.begin(), street_prefix.end(), ' '),street_prefix.end());
    lowerCase(street_prefix);
    int num_char = street_prefix.length();
    // find potential streets by range
    auto lower_bound = globals.ordered_street_name.lower_bound(street_prefix);
    std::string upper_bound_prefix = street_prefix;
    // increment last chacter to form upper bound
    upper_bound_prefix.back()++;
    auto upper_bound = globals.ordered_street_name.lower_bound(upper_bound_prefix);
    
    // if not found
    if(lower_bound == globals.ordered_street_name.end()){
        return found_streets;
    }
    // if found
    for (auto it = lower_bound; it != upper_bound; it++){
        std::string street_name = it->first;
        street_name .resize(num_char);
        if (street_name == street_prefix)
        found_streets.push_back(it->second);
    }

    return found_streets;
}

// Returns the length of a given street in meters.
double findStreetLength(StreetIdx street_id) {
    // if the street id is invalid
    if(street_id >= getNumStreets()){
        return 0.0;
    }
    
    double length = globals.vec_streetinfo[street_id].street_length;
    return length;
}

// Returns the nearest point of interest of the given name (e.g. "Starbucks") to the given position.
POIIdx findClosestPOI(LatLon my_position, std::string poi_name) {
    // call the helper function to go through all POI's and find the closest
    POIIdx index_of_closest_POI = loopThroughAllPOIs(my_position, poi_name);
    return (index_of_closest_POI);
}

// Returns the area of the given closed feature in square meters.
double findFeatureArea(FeatureIdx feature_id) {
    // local variables
    double area = 0;
    int num_of_feature_points = getNumFeaturePoints(feature_id);

    // check the coordinates of the first and last feature points to see if they are equal, to make sure it is a closed polygon
    if (getFeaturePoint(0, feature_id).latitude() == getFeaturePoint((num_of_feature_points - 1), feature_id).latitude() &&
        getFeaturePoint(0, feature_id) == getFeaturePoint((num_of_feature_points - 1),feature_id) && num_of_feature_points > 1) {
        // call the helper function to compute the area from all feature points in the vector that contains them
        area = getAreaFromFeaturePoints(num_of_feature_points, feature_id);
    }
    else {
        // if it's not a closed polygon
        return (0.0);
    }
    return (area);
}

// Returns the length of the OSMWay that has the given OSMID, in meters.
double findWayLength(OSMID way_id) {
    // access the data structure that has the distances of all nodes -> globals.way_distance -> unordered_map
    auto search = globals.way_distance.find(way_id);

    // check if the given way_id exists, return 0 if it doesn't
    if (search == globals.way_distance.end()) {
        std::cout << "Not found" << std::endl;
        return (0.0);
    }
    else {
        return (search->second);
    }
}

// Returns the value associated with this key on the specified OSMNode.
std::string getOSMNodeTagValue(OSMID osm_id, std::string key) {
    std::string node_string;
    const OSMNode* specified_node;

    // use the unordered_map find -> O(1)
    auto find_id = globals.node_to_id.find(osm_id);

    // if find_id is at the end of the map, return the empty string
    if (find_id == globals.node_to_id.end()) {
        return node_string;
    }
    else {
        specified_node = find_id->second;

        // go through all tag pairs for the given node, and see which one has the correct key, then get the pair
        for (int tag_index = 0; tag_index < getTagCount(specified_node); ++tag_index) {

            // get the pair from the function getTagPair
            std::pair <std::string, std::string> current_tags_from_node = getTagPair(specified_node, tag_index);
            if (current_tags_from_node.first == key) {
                return (current_tags_from_node.second);
            }
        }
        // if the key given isn't found in the node's tag pairs
        return node_string;
    }
}
