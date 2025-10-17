#include "astar.hpp"
#include "binary_loader/binary_database.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace gisevo {
    class BinaryDatabase;
}

namespace {

constexpr double kDegreeToRadian = 0.017453292519943295;
constexpr double kEarthRadiusInMeters = 6371000.0;

double distance_between_points(LatLon a, LatLon b) {
    double lat1 = a.latitude() * kDegreeToRadian;
    double lon1 = a.longitude() * kDegreeToRadian;
    double lat2 = b.latitude() * kDegreeToRadian;
    double lon2 = b.longitude() * kDegreeToRadian;

    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;

    double half_chord = std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
                        std::cos(lat1) * std::cos(lat2) *
                        std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
    double central_angle = 2.0 * std::atan2(std::sqrt(half_chord), std::sqrt(1.0 - half_chord));
    return kEarthRadiusInMeters * central_angle;
}

double street_segment_length(const gisevo::BinaryDatabase& db, StreetSegmentIdx segment_id) {
    const auto& node_refs = db.get_segment_node_refs(static_cast<std::size_t>(segment_id));
    const auto& node_lookup = db.get_node_id_to_index();

    std::vector<LatLon> points;
    points.reserve(node_refs.size());

    for (OSMID node_id : node_refs) {
        auto node_it = node_lookup.find(node_id);
        if (node_it == node_lookup.end()) {
            continue;
        }
        points.push_back(db.get_node_position(node_it->second));
    }

    if (points.size() < 2) {
        auto info = db.get_segment_info(static_cast<std::size_t>(segment_id));
        points.clear();
        points.push_back(db.get_intersection_position(info.from));
        points.push_back(db.get_intersection_position(info.to));
    }

    double length = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        length += distance_between_points(points[i - 1], points[i]);
    }
    return length;
}

} // namespace

std::vector<StreetSegmentIdx> findStreetSegmentsOfIntersection(const gisevo::BinaryDatabase& db, IntersectionIdx intersection_id) {
    std::vector<StreetSegmentIdx> segments;
    
    unsigned segment_count = db.get_intersection_street_segment_count(static_cast<std::size_t>(intersection_id));
    segments.reserve(segment_count);
    
    for (unsigned i = 0; i < segment_count; ++i) {
        StreetSegmentIdx segment_id = db.get_intersection_street_segment(i, static_cast<std::size_t>(intersection_id));
        segments.push_back(segment_id);
    }
    
    return segments;
}

double findStreetSegmentTravelTime(const gisevo::BinaryDatabase& db, StreetSegmentIdx segment_id) {
    double length = street_segment_length(db, segment_id);
    auto segment_info = db.get_segment_info(static_cast<std::size_t>(segment_id));
    
    // Convert speed limit from km/h to m/s
    double speed_ms = segment_info.speedLimit * (1000.0 / 3600.0);
    
    return length / speed_ms;
}

// Main A* algorithm implementation
std::vector<StreetSegmentIdx> aStarAlgorithm(
    const gisevo::BinaryDatabase& db,
    IntersectionIdx start_id,
    IntersectionIdx end_id,
    double turn_penalty
) {
    // vector for our path of nodes
    std::vector<StreetSegmentIdx> route_elements;

    // check if the start and end nodes are identical
    if (start_id == end_id) {
        return route_elements;
    }

    // holds a struct of nodes we have searched before
    std::vector<Search_Node> visited;
    visited.resize(db.get_intersection_count());

    LatLon end_pos = db.get_intersection_position(end_id);

    bool found_end = false; // used for regular A*

    // Find maximum speed limit for heuristic calculation
    double max_speed_ms = 0.0;
    for (std::size_t i = 0; i < db.get_segment_count(); ++i) {
        auto segment_info = db.get_segment_info(i);
        double speed_ms = segment_info.speedLimit * (1000.0 / 3600.0); // Convert km/h to m/s
        max_speed_ms = std::max(max_speed_ms, speed_ms);
    }

    // set up the first element, the start intersection
    Wave_Elm first_elm(start_id, 0, 0, 0, std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), 
                       distance_between_points(db.get_intersection_position(start_id), end_pos));

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

        if (static_cast<IntersectionIdx>(current_elm_id) == end_id) {
            found_end = true;
            int current_inter = end_id;
            int next_inter;

            while (static_cast<IntersectionIdx>(visited[current_inter].node_id) != static_cast<IntersectionIdx>(-1)) {
                route_elements.push_back(visited[current_inter].edge_id);
                next_inter = visited[current_inter].node_id;
                current_inter = next_inter;
            }
            std::reverse(route_elements.begin(), route_elements.end());
            return route_elements;
        }
        else {

            std::vector<StreetSegmentIdx> outgoing_streets = findStreetSegmentsOfIntersection(db, current_elm_id);

            // loop through all the outgoing streets of the current intersection (node)

            for (auto i: outgoing_streets) {
                bool invalid_check = false;

                IntersectionIdx next_intersection;

                auto segment_info = db.get_segment_info(static_cast<std::size_t>(i));

                // if the current node is at from, then the next node is at to
                // if the current node is at to, and it's not a one way street, then the next node is at from

                if (static_cast<IntersectionIdx>(current_elm_id) == segment_info.from) {
                    next_intersection = segment_info.to;
                } else if (!segment_info.oneWay) {
                    next_intersection = segment_info.from;
                } else {
                    invalid_check = true;
                }

                // if this node was popped from the wavefront before, no sense in checking it
                // if the road is one way in the wrong direction, skip it
                if (invalid_check || visited[next_intersection].visited) {
                    continue;
                }

                LatLon next_node_pos = db.get_intersection_position(next_intersection);
                double distance_to_next = street_segment_length(db, i);

                Search_Node next_node;
                next_node.edge_id = i;
                next_node.node_id = current_elm_id;

                // determine the best time to reach this node so far
                next_node.best_time = current_elm.travel_time + findStreetSegmentTravelTime(db, i);

                // account for the turn penalty if we change streets
                if (segment_info.streetID != current_elm.street_index) {
                    next_node.best_time += turn_penalty;
                }

                // only add this new node to the wavefront if we found a shorter route to it
                if (next_node.best_time < visited[next_intersection].best_time) {
                    visited[next_intersection] = next_node;
                    // get the distance to the destination from where we are now
                    double distance_to_end = distance_between_points(next_node_pos, end_pos);

                    double travel_time = next_node.best_time;

                    // distance is in m, max_speed is m/s
                    // guess the time it will take to get to the end
                    double time_to_end = distance_to_end / max_speed_ms;

                    // this incorporates the time taken to get to this node, plus the estimate time to the end using the max speed
                    double estimated_time = travel_time + time_to_end;

                    Wave_Elm next_elm(next_intersection, i, segment_info.streetID, travel_time,
                                      time_to_end,
                                      estimated_time, distance_to_end);

                    wave_front.push(next_elm);

                }
            }
        }
    }

    return route_elements;
}

// Main pathfinding function
std::vector<StreetSegmentIdx> findPathBetweenIntersections(
    const gisevo::BinaryDatabase& db,
    IntersectionIdx start_id,
    IntersectionIdx end_id,
    double turn_penalty
) {
    // calls algorithm function
    std::vector<StreetSegmentIdx> path = aStarAlgorithm(db, start_id, end_id, turn_penalty);
    return path;
}

// Path travel time computation
double computePathTravelTime(
    const gisevo::BinaryDatabase& db,
    const double turn_penalty,
    const std::vector<StreetSegmentIdx>& path
) {
    double total_time = 0;
    if (path.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    
    StreetIdx current_strt = db.get_segment_info(static_cast<std::size_t>(path[0])).streetID;
    for (int segment : path) {
        total_time += findStreetSegmentTravelTime(db, segment);
        auto segment_info = db.get_segment_info(static_cast<std::size_t>(segment));
        if (segment_info.streetID != current_strt) {
            total_time += turn_penalty;
            current_strt = segment_info.streetID;
        }
    }
    return total_time;
}
