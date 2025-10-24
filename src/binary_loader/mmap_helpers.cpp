#include "mmap_helpers.hpp"
#include <iostream>
#include <cstring>

namespace gisevo {
namespace mmap_helpers {

bool map_file(const std::string& path, std::unique_ptr<MappedFile>& mmap) {
    mmap = std::make_unique<MappedFile>();
    
    // Open file
    mmap->fd = open(path.c_str(), O_RDONLY);
    if (mmap->fd == -1) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return false;
    }
    
    // Get file size
    struct stat sb;
    if (fstat(mmap->fd, &sb) == -1) {
        std::cerr << "Failed to get file size: " << path << std::endl;
        return false;
    }
    mmap->size = sb.st_size;
    
    // Memory map the file
    mmap->data = ::mmap(nullptr, mmap->size, PROT_READ, MAP_PRIVATE, mmap->fd, 0);
    if (mmap->data == MAP_FAILED) {
        std::cerr << "Failed to memory map file: " << path << std::endl;
        return false;
    }
    
    return true;
}

std::string read_string_mmap(const char*& ptr) {
    const uint32_t length = read_pod_mmap<uint32_t>(ptr);
    std::string result(ptr, length);
    ptr += length;
    return result;
}

std::vector<OSMID> read_node_refs_mmap(const char*& ptr) {
    const uint32_t count = read_pod_mmap<uint32_t>(ptr);
    std::vector<OSMID> refs(count);
    if (count > 0) {
        std::memcpy(refs.data(), ptr, count * sizeof(OSMID));
        ptr += count * sizeof(OSMID);
    }
    return refs;
}

std::vector<std::pair<std::string, std::string>> read_tags_mmap(const char*& ptr) {
    const uint32_t tag_count = read_pod_mmap<uint32_t>(ptr);
    std::vector<std::pair<std::string, std::string>> tags;
    tags.reserve(tag_count);
    
    for (uint32_t i = 0; i < tag_count; ++i) {
        std::string key = read_string_mmap(ptr);
        std::string value = read_string_mmap(ptr);
        tags.emplace_back(std::move(key), std::move(value));
    }
    
    return tags;
}

} // namespace mmap_helpers
} // namespace gisevo
