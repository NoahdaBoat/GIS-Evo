#pragma once

#include <iosfwd>

namespace gisevo {

class BinaryDatabase;
class CacheManager;

namespace cache_serializer {

// Serialize entire database to stream
bool serialize_database(std::ostream& out, const BinaryDatabase& db);

// Deserialize entire database from stream
bool deserialize_database(std::istream& in, BinaryDatabase& db, 
                         double min_lat, double max_lat, 
                         double min_lon, double max_lon, 
                         double avg_lat_rad);

} // namespace cache_serializer
} // namespace gisevo
