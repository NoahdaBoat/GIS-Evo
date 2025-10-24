#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "StreetsDatabaseAPI.h"

namespace gisevo {
namespace mmap_helpers {

// Memory-mapped file structure
struct MappedFile {
    void* data = nullptr;
    size_t size = 0;
    int fd = -1;
    
    ~MappedFile() {
        if (data && data != MAP_FAILED) {
            munmap(data, size);
        }
        if (fd != -1) {
            close(fd);
        }
    }
};

// Map a file into memory
bool map_file(const std::string& path, std::unique_ptr<MappedFile>& mmap);

// Template function to read POD types from memory-mapped data
template <typename T>
T read_pod_mmap(const char*& ptr) {
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

// Read string from memory-mapped data
std::string read_string_mmap(const char*& ptr);

// Read vector of OSMIDs from memory-mapped data
std::vector<OSMID> read_node_refs_mmap(const char*& ptr);

// Read tag vector from memory-mapped data
std::vector<std::pair<std::string, std::string>> read_tags_mmap(const char*& ptr);

} // namespace mmap_helpers
} // namespace gisevo
