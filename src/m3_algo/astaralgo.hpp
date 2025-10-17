#ifndef ASTARALGO_HPP
#define ASTARALGO_HPP

#include <vector>
#include "../m3.h"

// Function declaration for A* algorithm (matches m3.h signature)
std::vector<StreetSegmentIdx> aStarAlgorithm(IntersectionIdx start, IntersectionIdx end, double turn_penalty);

#endif // ASTARALGO_HPP