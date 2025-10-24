#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
#include "StreetsDatabaseAPI.h"

namespace gisevo {
namespace cache_io {

// Constants
constexpr std::size_t kMagicLength = 8;

// Basic I/O operations
bool read_exact(std::istream& in, char* buffer, std::size_t length);
bool write_exact(std::ostream& out, const char* buffer, std::size_t length);

// String I/O
bool read_string(std::istream& in, std::string& value);
bool write_string(std::ostream& out, const std::string& value);

// Vector size I/O
bool read_vector_size(std::istream& in, std::size_t& size);
bool write_vector_size(std::ostream& out, std::size_t size);

// String vector I/O
bool read_string_vector(std::istream& in, std::vector<std::string>& vec);
bool write_string_vector(std::ostream& out, const std::vector<std::string>& vec);

// Tag vector I/O
bool read_tag_vector(std::istream& in, std::vector<std::pair<std::string, std::string>>& tags);
bool write_tag_vector(std::ostream& out, const std::vector<std::pair<std::string, std::string>>& tags);

// OSMID vector I/O
bool read_osmid_vector(std::istream& in, std::vector<OSMID>& vec);
bool write_osmid_vector(std::ostream& out, const std::vector<OSMID>& vec);

} // namespace cache_io
} // namespace gisevo
