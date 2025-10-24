#include "cache_io_helpers.hpp"
#include <istream>
#include <ostream>

namespace gisevo {
namespace cache_io {

bool read_exact(std::istream& in, char* buffer, std::size_t length) {
    in.read(buffer, static_cast<std::streamsize>(length));
    return in.good();
}

bool write_exact(std::ostream& out, const char* buffer, std::size_t length) {
    out.write(buffer, static_cast<std::streamsize>(length));
    return out.good();
}

bool write_string(std::ostream& out, const std::string& value) {
    const std::uint32_t size = static_cast<std::uint32_t>(value.size());
    out.write(reinterpret_cast<const char*>(&size), sizeof(size));
    out.write(value.data(), size);
    return out.good();
}

bool read_string(std::istream& in, std::string& value) {
    std::uint32_t size = 0;
    in.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!in.good()) {
        return false;
    }
    value.resize(size);
    in.read(value.data(), size);
    return in.good();
}

bool write_vector_size(std::ostream& out, std::size_t size) {
    const std::uint64_t size_64 = static_cast<std::uint64_t>(size);
    out.write(reinterpret_cast<const char*>(&size_64), sizeof(size_64));
    return out.good();
}

bool read_vector_size(std::istream& in, std::size_t& size) {
    std::uint64_t size_64;
    in.read(reinterpret_cast<char*>(&size_64), sizeof(size_64));
    size = static_cast<std::size_t>(size_64);
    return in.good();
}

bool write_string_vector(std::ostream& out, const std::vector<std::string>& vec) {
    if (!write_vector_size(out, vec.size())) return false;
    for (const auto& str : vec) {
        if (!write_string(out, str)) return false;
    }
    return out.good();
}

bool read_string_vector(std::istream& in, std::vector<std::string>& vec) {
    std::size_t size;
    if (!read_vector_size(in, size)) return false;
    vec.resize(size);
    for (std::size_t i = 0; i < size; ++i) {
        if (!read_string(in, vec[i])) return false;
    }
    return in.good();
}

bool write_tag_vector(std::ostream& out, const std::vector<std::pair<std::string, std::string>>& tags) {
    if (!write_vector_size(out, tags.size())) return false;
    for (const auto& tag : tags) {
        if (!write_string(out, tag.first)) return false;
        if (!write_string(out, tag.second)) return false;
    }
    return out.good();
}

bool read_tag_vector(std::istream& in, std::vector<std::pair<std::string, std::string>>& tags) {
    std::size_t size;
    if (!read_vector_size(in, size)) return false;
    tags.resize(size);
    for (std::size_t i = 0; i < size; ++i) {
        if (!read_string(in, tags[i].first)) return false;
        if (!read_string(in, tags[i].second)) return false;
    }
    return in.good();
}

bool write_osmid_vector(std::ostream& out, const std::vector<OSMID>& vec) {
    if (!write_vector_size(out, vec.size())) return false;
    if (!vec.empty()) {
        out.write(reinterpret_cast<const char*>(vec.data()), vec.size() * sizeof(OSMID));
    }
    return out.good();
}

bool read_osmid_vector(std::istream& in, std::vector<OSMID>& vec) {
    std::size_t size;
    if (!read_vector_size(in, size)) return false;
    vec.resize(size);
    if (!vec.empty()) {
        in.read(reinterpret_cast<char*>(vec.data()), vec.size() * sizeof(OSMID));
    }
    return in.good();
}

} // namespace cache_io
} // namespace gisevo
