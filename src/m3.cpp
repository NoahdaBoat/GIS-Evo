//
// Created by helanlin on 3/22/24.
//
#include "m3.h"
#include "m1.h"
#include "globals.h"
#include "astaralgo.hpp"
#include <chrono>
#include <iostream>

int timesum = 0;

// Returns the time required to travel along the path specified, in seconds.
// The path is given as a vector of street segment ids, and this function can
// assume the vector either forms a legal path or has size == 0. The travel
// time is the sum of the length/speed-limit of each street segment, plus the
// given turn_penalty (in seconds) per turn implied by the path. If there is
// no turn, then there is no penalty. Note that whenever the street id changes
// (e.g. going from Bloor Street West to Bloor Street East) we have a turn.
double computePathTravelTime(const double turn_penalty, const std::vector<StreetSegmentIdx>& path){
    double total_time =0;
    //directly return if no street segments
    if(path.empty()) {
        return INFINITY;
    }
    StreetIdx current_strt = globals.all_street_segments[path[0]].street;
    for(int segment : path) {
        total_time += findStreetSegmentTravelTime(segment);
        if(globals.all_street_segments[segment].street !=current_strt ) {
            total_time += turn_penalty;
            //also update current street name since changed to a new street
            current_strt = globals.all_street_segments[segment].street;
        }
    }
    return total_time;
}

// Returns a path (route) between the start intersection (intersect_id.first)
// and the destination intersection (intersect_id.second), if one exists.
// This routine should return the shortest path
// between the given intersections, where the time penalty to turn right or
// left is given by turn_penalty (in seconds). If no path exists, this routine
// returns an empty (size == 0) vector. If more than one path exists, the path
// with the shortest travel time is returned. The path is returned as a vector
// of street segment ids; traversing these street segments, in the returned
// order, would take one from the start to the destination intersection.
std::vector<StreetSegmentIdx> findPathBetweenIntersections(const double turn_penalty, const std::pair<IntersectionIdx, IntersectionIdx> intersect_ids) {

    // calls algorithm function
    std::vector<StreetSegmentIdx> path = aStarAlgorithm(intersect_ids.first, intersect_ids.second, turn_penalty);
    return path;
}
