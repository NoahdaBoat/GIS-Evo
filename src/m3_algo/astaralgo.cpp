//
// Created by montinoa on 3/22/24.
//

#include "astaralgo.hpp"
#include "StreetsDatabaseAPI.h"
#include "m1.h"
#include "globals.h"
#include <vector>
#include <queue>
#include <chrono>


/*
 * Explanation:
 * This algorithm finds the shortest route between two intersections, and can account for a turn penalty
 *
 * It uses Dijkstra's algorithm with a heuristic (AKA A*)
 *
 * Heuristic: The Euclidean distance to the destination intersection from the current intersection, divided by the maximum speed limit
 * of the map (as to not overestimate, which is a requirement of A*), plus the time taken to reach this current intersection
 *
 * The wave front is a priority queue where the node with the lowest heuristic value is searched next
 *
 * The searched elements vector contains information on nodes once they have been searched, most importantly the street segment used to reached that
 * node, which allows us to reconstruct the route after the destination has been found
 */

// Note: Node and Intersection mean the same thing here
std::vector<StreetSegmentIdx> aStarAlgorithm(int start_id, int end_id, int turn_penalty) {

    // vector for our path of nodes
    std::vector<StreetSegmentIdx> route_elements;

    // check if the start and end nodes are identical
    if (start_id == end_id) {
        return route_elements;
    }

    // holds a struct of nodes we have searched before
    std::vector<Search_Node> visited;
    visited.resize(getNumIntersections());


    LatLon end_pos = getIntersectionPosition(end_id);

    bool found_end = false; // used for regular A*

    // set up the first element, the start intersection
    Wave_Elm first_elm(start_id, 0, 0, 0, std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), findDistanceBetweenTwoPoints(getIntersectionPosition(start_id), end_pos));

    // already searched the beginning intersection
    Search_Node first_node;
    first_node.edge_id = 0; // will be incorrect for the first node
    first_node.best_time = 0;
    first_node.node_id = -1;

    visited[start_id] = first_node;

    // queue of nodes to search
    std::priority_queue<Wave_Elm, std::vector<Wave_Elm>, comparator> wave_front;

    wave_front.push(first_elm);

    // loop until the queue is empty or the node is found
    while (!wave_front.empty() && !found_end) {

        Wave_Elm current_elm = wave_front.top();

        if (!wave_front.empty()) {
            wave_front.pop();
        }

        int current_elm_id = current_elm.node_id;

        if (visited[current_elm_id].visited) {
            continue;
        }

        visited[current_elm_id].visited = true;

        if (current_elm_id == end_id) {
            found_end = true;
            int current_inter = end_id;
            int next_inter;

            while (visited[current_inter].node_id != -1) {
                route_elements.push_back(visited[current_inter].edge_id);
                next_inter = visited[current_inter].node_id;
                current_inter = next_inter;
            }
            std::reverse(route_elements.begin(), route_elements.end());
            return route_elements;
        }
        else {

            std::vector<StreetSegmentIdx> outgoing_streets = findStreetSegmentsOfIntersection(current_elm_id);

            // loop through all the outgoing streets of the current intersection (node)

            for (auto i: outgoing_streets) {
                bool invalid_check = false;

                IntersectionIdx next_intersection;

                // if the current node is at from, then the next node is at to
                // if the current node is at to, and it's not a one way street, then the next node is at from

                if (current_elm_id == globals.all_street_segments[i].from) {
                    next_intersection = globals.all_street_segments[i].to;
                } else if (!globals.all_street_segments[i].oneWay) {
                    next_intersection = globals.all_street_segments[i].from;
                } else {
                    invalid_check = true;
                }

                // if this node was popped from the wavefront before, no sense in checking it
                // if the road is one way in the wrong direction, skip it
                if (invalid_check || visited[next_intersection].visited) {
                    continue;
                }

                LatLon next_node_pos = getIntersectionPosition(next_intersection);
                double distance_to_next = findStreetSegmentLength(i);

                Search_Node next_node;
                next_node.edge_id = i;
                next_node.node_id = current_elm_id;

                // determine the best time to reach this node so far
                next_node.best_time =
                        current_elm.travel_time + distance_to_next / globals.all_street_segments[i].speedLimit;

                // account for the turn penalty if we change streets
                if (globals.all_street_segments[i].street != current_elm.street_index) {
                    next_node.best_time += turn_penalty;
                }

                // only add this new node to the wavefront if we found a shorter route to it
                if (next_node.best_time < visited[next_intersection].best_time) {
                    visited[next_intersection] = next_node;
                    // get the distance to the destination from where we are now
                    double distance_to_end = findDistanceBetweenTwoPoints(next_node_pos, end_pos);

                    double travel_time = next_node.best_time;

                    // distance is in m, max_speed is m/s
                    // guess the time it will take to get to the end
                    double time_to_end = distance_to_end / globals.max_speed;

                    // this incorporates the time taken to get to this node, plus the estimate time to the end using the max speed
                    double estimated_time = travel_time + time_to_end;

                    Wave_Elm next_elm(next_intersection, i, globals.all_street_segments[i].street, travel_time,
                                      time_to_end,
                                      estimated_time, distance_to_end);

                    wave_front.push(next_elm);

                }
            }
        }
    }

    return route_elements;
}