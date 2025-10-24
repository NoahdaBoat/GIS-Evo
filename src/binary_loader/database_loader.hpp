#pragma once

#include <memory>
#include <string>

namespace gisevo {

class BinaryDatabase;

namespace database_loader {

// Load streets file using memory-mapped I/O (primary method)
bool load_streets_file(BinaryDatabase& db, const std::string& path);

// Load OSM file using memory-mapped I/O (primary method)
bool load_osm_file(BinaryDatabase& db, const std::string& path);

// Fallback: Load streets file using stream I/O
bool load_streets_file_stream(BinaryDatabase& db, const std::string& path);

// Fallback: Load OSM file using stream I/O
bool load_osm_file_stream(BinaryDatabase& db, const std::string& path);

} // namespace database_loader
} // namespace gisevo
