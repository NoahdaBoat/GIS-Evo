#include <cmath>
#include <vector>
#include <unordered_map>
#include <string>
#include "m1.h"
#include "StreetsDatabaseAPI.h"
#include "OSMDatabaseAPI.h"
#include "ms1helpers.h"
#include "globals.h"
#include "struct.h"
#include "coords_conversions.hpp"

void lowerCase(std::string& Orin_String) {
    // loop through all characters within the string
    for (int i = 0; i < Orin_String.size(); i++) {
        // check if current character is capitalized
        if (Orin_String[i] >= 'A' && Orin_String[i] <= 'Z') {
            Orin_String[i] = Orin_String[i] + 32;
        }
    }
}

void mapOSMIDToNode() {
    int numOfNodes = getNumberOfNodes();
    for (int i = 0; i < numOfNodes; ++i) {
        const OSMNode* tempNode = getNodeByIndex(i);
        OSMID id = tempNode->id();
        globals.node_to_id.insert({id, tempNode});
    }
}

void mapOSMIDToWay() {
    for (int i = 0; i < getNumberOfWays(); ++i) {
        const OSMWay* temp_way = getWayByIndex(i);
        OSMID id = temp_way->id();
        globals.id_to_way.insert({id, temp_way});
    }
}

void mapOSMIDToRelation() {
    for (int i = 0; i < getNumberOfRelations(); ++i) {
        const OSMRelation* temp_relation = getRelationByIndex(i);
        OSMID id = temp_relation->id();
        globals.id_to_relation.insert({id, temp_relation});
    }
}

void replaceString(std::string& currentStr, const std::string& toReplace, const std::string& replaceWith) {
    std::size_t idx = currentStr.find(toReplace);
    if (idx != std::string::npos) {
        currentStr.replace(idx, toReplace.length(), replaceWith);
    } 
}


void preLoadWayDistance() {
    std::vector <OSMID> way_nodes;
    int numberOfWays = getNumberOfWays();
    double distance = 0;
    const OSMNode* node1;
    const OSMNode* node2;
    const OSMWay* way;
    OSMID wayid;
    int j;

    // outer for loop to go through all OSMWays in the map
    for (int i = 0; i < numberOfWays; ++i) {
        way = getWayByIndex(i);
        way_nodes = getWayMembers(way);
        wayid = way->id();
        // this loop is to go through each node
        auto nodesInWay = way_nodes.size();
        distance = 0;

        // inner for loop to go through all OSMIDs/OSMNodes contained in each way
        for (j = 0; j < nodesInWay - 1; ++j) {
            // look for the corresponding node from the id
            auto search = globals.node_to_id.find(way_nodes[j]);

            // if the id isn't found to be a node, then go to the next (should never occur assuming correct data)
            if (search != globals.node_to_id.end()) {
                node1 = search->second;
                search = globals.node_to_id.find(way_nodes[j+1]);
                if (search != globals.node_to_id.end()) {
                    node2 = search->second;
                    LatLon point1 = getNodeCoords(node1);
                    LatLon point2 = getNodeCoords(node2);
                    distance += findDistanceBetweenTwoPoints(point1, point2);
                }
            }
        }
        if (isClosedWay(way) && j+1 != nodesInWay) { // distance from end to start for closed objects
            auto search = globals.node_to_id.find(way_nodes[nodesInWay]);
            if (search != globals.node_to_id.end()) {
                node1 = search->second;
                search = globals.node_to_id.find(way_nodes[0]);
                if (search != globals.node_to_id.end()) {
                    node2 = search->second;
                    LatLon point1 = getNodeCoords(node1);
                    LatLon point2 = getNodeCoords(node2);
                    distance += findDistanceBetweenTwoPoints(point1, point2);
                }
            }
        }
        globals.way_distance.insert({wayid, distance});

    }
}

double latAverageOfFeature(std::vector<LatLon>& featureListOfLatLon) {
    double avg = 0;
    for (auto i : featureListOfLatLon) {
        avg += i.latitude();
    }
    avg /= featureListOfLatLon.size();
    avg *= kDegreeToRadian;
    return avg;
}

void convertLatLonToXY(LatLon point, double& x, double& y, double& avg) {
    // uses constants defined in m1.h
    double lat = kDegreeToRadian * point.latitude();
    double lon = kDegreeToRadian * point.longitude();
    // cos from cmath
    x = kEarthRadiusInMeters * lon * cos(avg);
    y = kEarthRadiusInMeters * lat;
}

double trapezoidRule(double& x1, double& y1, double& x2, double& y2) {
    double area = ((y1+y2)*(x1-x2));
    return area;
}

void loopAllStreetSegments(){
    globals.max_speed = 0;
    int num_street_segment = getNumStreetSegments();
    for (StreetSegmentIdx i = 0; i < num_street_segment; ++i){
        // preload globals.vec_streetinfo
        // preload intersections
        StreetSegmentInfo street_segment_info = getStreetSegmentInfo(i);
        globals.vec_streetinfo[street_segment_info.streetID].intersections.push_back(street_segment_info.from);
        globals.vec_streetinfo[street_segment_info.streetID].intersections.push_back(street_segment_info.to);
        
        // preload street segments
        globals.vec_streetinfo[street_segment_info.streetID].street_segments.push_back(i);
        
        // preload street length
        StreetSegmentDistance ss_dis;
        double ss_length = CalculateSSLength(i);
        globals.vec_streetinfo[street_segment_info.streetID].street_length += ss_length;

        // set the max speed global variable
        if (street_segment_info.speedLimit > globals.max_speed) {
            globals.max_speed = street_segment_info.speedLimit;
        }

        // preload globals.vec_segmentdis
        ss_dis.segment_length = ss_length;
        // avoid dividing by 0
        if (street_segment_info.speedLimit == 0){
            ss_dis.travel_time = 0;
        }
        else {
            ss_dis.travel_time = ss_length / street_segment_info.speedLimit;
        }
        globals.vec_segmentdis.push_back(ss_dis);
    }

    // remove duplicates for each street's intersection list
    for (auto& street : globals.vec_streetinfo){
        std::sort(street.intersections.begin(), street.intersections.end());
        street.intersections.erase(std::unique(street.intersections.begin(), street.intersections.end()), street.intersections.end());
    }   
}

// void preLoadAjacentIntersections(){

//     // loop through all intersections 
//     int num_intersections = getNumIntersections();
//     for (IntersectionIdx i = 0; i < num_intersections; i++){

//         // loop through all directly connected street segments to the intersection
//         int num_street_segments = getNumIntersectionStreetSegment(i);
//         for (StreetSegmentIdx j = 0; j < num_street_segments; j++){

//             // push back the intersection across from the directly connected street segment 
//             StreetSegmentInfo segment_info = getStreetSegmentInfo(getIntersectionStreetSegment(j, i));
//             if (segment_info.from != i){
//                 globals.adjacent_intersections[i].push_back(segment_info.from);
//             }
//             else {
//                 globals.adjacent_intersections[i].push_back(segment_info.to);
//             }
//         }
//     }
// }

double CalculateSSLength(StreetSegmentIdx street_segment_id) {
    StreetSegmentInfo input_segment = getStreetSegmentInfo(street_segment_id);
    double length = 0;
    
    // get the Latlon of intersection from and to
    IntersectionIdx intersection_from = input_segment.from;
    IntersectionIdx intersection_to = input_segment.to;
    LatLon position_begin = getIntersectionPosition(intersection_from);
    LatLon position_end = getIntersectionPosition(intersection_to);

    int num_Curve_pt = input_segment.numCurvePoints;
    LatLon old_point = position_begin;
    // if no curve points, the distance can be direclty caculated with two intersection points
    if(num_Curve_pt == 0){
        length = findDistanceBetweenTwoPoints(position_begin, position_end);
    }
    else {
        // loop through all curve points to calculate segment length
        for(int i = 0; i < num_Curve_pt; i++){
            LatLon new_point = getStreetSegmentCurvePoint(i, street_segment_id);
            length += findDistanceBetweenTwoPoints(old_point, new_point);
            // update old point
            old_point = new_point;
        }
        // add in the length between last curve point and intersection_to
        length += findDistanceBetweenTwoPoints(old_point, position_end);
    }
    return length;
}

void preLoadIntersectionStreetSegment(){
    // resize the vector 
    globals.intersection_street_segments.resize(getNumIntersections());
    int num_intersections = getNumIntersections();
    // loop through all intersections
    for(IntersectionIdx intersection = 0; intersection < num_intersections; ++intersection) {
        // loop through all street segments linked to current intersection
        int num_street_segments = getNumIntersectionStreetSegment(intersection);
        for (int i = 0; i < num_street_segments; ++i) {
            //load intersection street segment
            int ss_id = getIntersectionStreetSegment(i, intersection);
            globals.intersection_street_segments[intersection].push_back(ss_id);

            //load adjacent intersections
            StreetSegmentInfo segment_info = getStreetSegmentInfo(getIntersectionStreetSegment(i, intersection));
            if (segment_info.from != intersection){
                globals.adjacent_intersections[intersection].push_back(segment_info.from);
            }
            else {
                globals.adjacent_intersections[intersection].push_back(segment_info.to);
            }
        }
    }    
}

void loopAllStreets() {
    for (StreetIdx street_id = 0; street_id < getNumStreets(); ++street_id){
        // load all streets into a multimap in alphabetical order with their name
        std::string strt_name = getStreetName(street_id);
        // remove all the spaces in street names
        strt_name.erase(std::remove(strt_name.begin(), strt_name.end(), ' '),strt_name.end());
        // put names in lower cases
        lowerCase(strt_name);
        globals.ordered_street_name.insert(std::make_pair(strt_name, street_id));

        // initialize a struct for streetInfo
        StreetsInfo a_street;
        // initialize street length to 0
        a_street.street_length = 0.0;
        globals.vec_streetinfo.push_back(a_street);
    }
}

POIIdx loopThroughAllPOIs(LatLon& my_position, std::string& poi_name) {
    int number_of_POIs = getNumPointsOfInterest();

    // set the closest_point to the farthest away by default
    double current_smallest_distance = findDistanceBetweenTwoPoints(my_position,LatLon(std::numeric_limits<float>::max(),std::numeric_limits<float>::max()));
    LatLon current_poi_loc;
    POIIdx index_of_closest_POI = 0;
    double found_distance;

    // go through every POI, check if the name matches, then see if it's distance away from my_position is closer than the current closest
    for (POIIdx current_POIidx = 0; current_POIidx < number_of_POIs; ++current_POIidx) {
        std::string tester = getPOIName(current_POIidx);
        if (getPOIName(current_POIidx) == poi_name) {
            current_poi_loc = getPOIPosition(current_POIidx);
            found_distance = findDistanceBetweenTwoPoints(my_position, current_poi_loc);
            if (found_distance < current_smallest_distance) {
                index_of_closest_POI = current_POIidx;
                current_smallest_distance = found_distance;
            }
        }
    }
    return index_of_closest_POI;
}

double getAreaFromFeaturePoints(int& num_of_feature_points, FeatureIdx& feature_id) {
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    std::vector<LatLon> feature_list_of_latlon;
    double area = 0;

    for (int i = 0; i < num_of_feature_points; ++i) {
        feature_list_of_latlon.push_back(getFeaturePoint(i, feature_id));
    }

    double averageLat = latAverageOfFeature(feature_list_of_latlon);
    int featurePoints = getNumFeaturePoints(feature_id) - 1;

    // loop through all points of the feature and call the trapezoidRule function to calculate the area
    for (int i = 0; i < featurePoints; ++i) {
        convertLatLonToXY(getFeaturePoint(i, feature_id), x1, y1, averageLat);
        convertLatLonToXY(getFeaturePoint(i+1, feature_id), x2, y2, averageLat);
        area += trapezoidRule(x1, y1, x2, y2);
    }

    // set the last point to the first point for the final calculation
    x1 = x2;
    y1 = y2;
    convertLatLonToXY(getFeaturePoint(0, feature_id), x2, y2, averageLat);

    // add the area between the last and first points
    area += trapezoidRule(x1, y1, x2, y2);

    // if the formula went the wrong direction (CCW instead of CW), then we need to take the absolute value of the area
    if (area < 0) {
        area = fabs(area);
    }
    area /= 2;
    return area;
}
