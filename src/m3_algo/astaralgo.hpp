#ifndef ASTARALGO_HPP
#define ASTARALGO_HPP

#include <vector>
#include "../StreetsDatabaseAPI.h"

// Function declarations for A* algorithm
std::vector<IntersectionIdx> findPathBetweenIntersections(IntersectionIdx start, IntersectionIdx end);

#endif // ASTARALGO_HPP