#include "converter/converter.hpp"

#include "converter/schema.hpp"

#include <osmium/io/pbf_input.hpp>
#include <osmium/io/reader.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/tag.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/visitor.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace gisevo::converter {
namespace {

using osm_id = osmium::object_id_type;

struct ConverterDataInternal {
  ConverterData data;
  std::unordered_set<osm_id> referenced_nodes;
  std::unordered_map<osm_id, std::size_t> node_index_lookup;
  std::vector<osm_id> missing_node_ids;
};

std::string to_lower_copy(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

HighwayCategory encode_highway_category(const char* value) {
  if (value == nullptr) {
    return HighwayCategory::kUnknown;
  }
  const std::string lower = to_lower_copy(value);
  if (lower == "motorway") return HighwayCategory::kMotorway;
  if (lower == "motorway_link") return HighwayCategory::kMotorway;
  if (lower == "trunk") return HighwayCategory::kTrunk;
  if (lower == "trunk_link") return HighwayCategory::kTrunk;
  if (lower == "primary") return HighwayCategory::kPrimary;
  if (lower == "primary_link") return HighwayCategory::kPrimary;
  if (lower == "secondary") return HighwayCategory::kSecondary;
  if (lower == "secondary_link") return HighwayCategory::kSecondary;
  if (lower == "tertiary") return HighwayCategory::kTertiary;
  if (lower == "tertiary_link") return HighwayCategory::kTertiary;
  if (lower == "residential") return HighwayCategory::kResidential;
  if (lower == "living_street") return HighwayCategory::kResidential;
  if (lower == "service") return HighwayCategory::kService;
  if (lower == "track") return HighwayCategory::kTrack;
  if (lower == "footway") return HighwayCategory::kFootway;
  if (lower == "pedestrian") return HighwayCategory::kFootway;
  if (lower == "path") return HighwayCategory::kPath;
  if (lower == "cycleway") return HighwayCategory::kCycleway;
  return HighwayCategory::kUnknown;
}

float parse_max_speed(const osmium::TagList& tags) {
  const char* raw_value = tags.get_value_by_key("maxspeed");
  if (!raw_value) {
    return -1.0F;
  }
  std::string value = raw_value;
  value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
  bool mph = false;
  if (value.size() > 3) {
    const std::string suffix = to_lower_copy(std::string_view(value).substr(value.size() - 3));
    if (suffix == "mph") {
      mph = true;
      value.resize(value.size() - 3);
    }
  }
  try {
    const float numeric = std::stof(value);
    if (mph) {
      return numeric * 1.60934F;
    }
    return numeric;
  } catch (const std::exception&) {
    return -1.0F;
  }
}

FeatureType encode_feature_type(const osmium::TagList& tags) {
  // Check for natural features
  if (const char* natural = tags.get_value_by_key("natural")) {
    const std::string lower = to_lower_copy(natural);
    if (lower == "water" || lower == "lake" || lower == "pond" || lower == "reservoir") {
      return FeatureType::kWater;
    }
    if (lower == "forest" || lower == "wood") {
      return FeatureType::kForest;
    }
    if (lower == "grassland" || lower == "meadow") {
      return FeatureType::kGrassland;
    }
    if (lower == "wetland" || lower == "marsh") {
      return FeatureType::kWetland;
    }
    if (lower == "beach") {
      return FeatureType::kBeach;
    }
    if (lower == "coastline") {
      return FeatureType::kCoastline;
    }
  }
  
  // Check for leisure features
  if (const char* leisure = tags.get_value_by_key("leisure")) {
    const std::string lower = to_lower_copy(leisure);
    if (lower == "park") {
      return FeatureType::kPark;
    }
    if (lower == "garden") {
      return FeatureType::kGarden;
    }
    if (lower == "playground") {
      return FeatureType::kPlayground;
    }
    if (lower == "stadium") {
      return FeatureType::kStadium;
    }
  }
  
  // Check for landuse features
  if (const char* landuse = tags.get_value_by_key("landuse")) {
    const std::string lower = to_lower_copy(landuse);
    if (lower == "forest") {
      return FeatureType::kForest;
    }
    if (lower == "grass" || lower == "grassland") {
      return FeatureType::kGrassland;
    }
    if (lower == "park") {
      return FeatureType::kPark;
    }
    if (lower == "cemetery") {
      return FeatureType::kCemetery;
    }
  }
  
  // Check for amenity features
  if (const char* amenity = tags.get_value_by_key("amenity")) {
    const std::string lower = to_lower_copy(amenity);
    if (lower == "hospital") {
      return FeatureType::kHospital;
    }
    if (lower == "school") {
      return FeatureType::kSchool;
    }
    if (lower == "university" || lower == "college") {
      return FeatureType::kUniversity;
    }
  }
  
  // Check for building features
  if (tags.has_key("building")) {
    return FeatureType::kBuilding;
  }
  
  // Check for waterway features
  if (const char* waterway = tags.get_value_by_key("waterway")) {
    const std::string lower = to_lower_copy(waterway);
    if (lower == "river") {
      return FeatureType::kRiver;
    }
    if (lower == "stream") {
      return FeatureType::kStream;
    }
    if (lower == "canal") {
      return FeatureType::kCanal;
    }
  }
  
  // Check for aeroway features
  if (tags.has_key("aeroway")) {
    return FeatureType::kAirport;
  }
  
  // Check for railway features
  if (tags.has_key("railway")) {
    return FeatureType::kRailway;
  }
  
  // Check for bridge/tunnel features
  if (tags.has_key("bridge")) {
    return FeatureType::kBridge;
  }
  if (tags.has_key("tunnel")) {
    return FeatureType::kTunnel;
  }
  
  // Check for barrier features
  if (const char* barrier = tags.get_value_by_key("barrier")) {
    const std::string lower = to_lower_copy(barrier);
    if (lower == "wall") {
      return FeatureType::kWall;
    }
    if (lower == "fence") {
      return FeatureType::kFence;
    }
    return FeatureType::kBarrier;
  }
  
  return FeatureType::kUnknown;
}

std::optional<std::string> detect_poi_category(const osmium::TagList& tags) {
  if (const char* amenity = tags.get_value_by_key("amenity")) {
    return std::string("amenity:") + amenity;
  }
  if (const char* shop = tags.get_value_by_key("shop")) {
    return std::string("shop:") + shop;
  }
  if (const char* tourism = tags.get_value_by_key("tourism")) {
    return std::string("tourism:") + tourism;
  }
  if (const char* leisure = tags.get_value_by_key("leisure")) {
    return std::string("leisure:") + leisure;
  }
  if (const char* railway = tags.get_value_by_key("railway")) {
    return std::string("railway:") + railway;
  }
  if (const char* public_transport = tags.get_value_by_key("public_transport")) {
    return std::string("public_transport:") + public_transport;
  }
  if (const char* highway = tags.get_value_by_key("highway")) {
    const std::string lower = to_lower_copy(highway);
    if (lower == "bus_stop" || lower == "tram_stop" || lower == "platform") {
      return std::string("highway:") + lower;
    }
  }
  if (const char* aeroway = tags.get_value_by_key("aeroway")) {
    return std::string("aeroway:") + aeroway;
  }
  return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> extract_tags(const osmium::TagList& tags) {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(tags.size());
  
  for (const auto& tag : tags) {
    result.emplace_back(std::string(tag.key()), std::string(tag.value()));
  }
  
  return result;
}

class HighwayCollector final : public osmium::handler::Handler {
 public:
  explicit HighwayCollector(ConverterDataInternal& internal)
    : internal_(internal) {}

  void way(const osmium::Way& way) {
    const char* highway = way.tags().get_value_by_key("highway");
    if (!highway) {
      return;
    }

    StreetSegmentRecord record;
    record.osm_id = way.id();
    record.category = encode_highway_category(highway);
    record.max_speed_kph = parse_max_speed(way.tags());
    record.tags = extract_tags(way.tags());

    if (const char* name = way.tags().get_value_by_key("name")) {
      record.name = name;
    }

    for (const auto& node_ref : way.nodes()) {
      internal_.referenced_nodes.insert(node_ref.ref());
      record.node_refs.push_back(node_ref.ref());
    }

    if (record.node_refs.size() < 2) {
      return;
    }

    internal_.data.street_segments.emplace_back(std::move(record));
  }

 private:
  ConverterDataInternal& internal_;
};

class FeatureCollector final : public osmium::handler::Handler {
 public:
  explicit FeatureCollector(ConverterDataInternal& internal)
    : internal_(internal) {}

  void way(const osmium::Way& way) {
    FeatureType feature_type = encode_feature_type(way.tags());
    if (feature_type == FeatureType::kUnknown) {
      return;
    }

    FeatureRecord record;
    record.osm_id = way.id();
    record.type = feature_type;
    record.is_closed = way.is_closed();
    record.tags = extract_tags(way.tags());
    
    if (const char* name = way.tags().get_value_by_key("name")) {
      record.name = name;
    }

    // Store node references
    for (const auto& node_ref : way.nodes()) {
      record.node_refs.push_back(node_ref.ref());
      internal_.referenced_nodes.insert(node_ref.ref());
    }

    internal_.data.features.emplace_back(std::move(record));
  }

 private:
  ConverterDataInternal& internal_;
};

class NodeCollector final : public osmium::handler::Handler {
 public:
  explicit NodeCollector(ConverterDataInternal& internal)
      : internal_(internal) {}

  void node(const osmium::Node& node) {
    if (!node.location().valid()) {
      return;
    }

    const osm_id id = node.id();

    if (internal_.referenced_nodes.contains(id)) {
      NodeRecord record;
      record.osm_id = id;
      record.lat = node.location().lat();
      record.lon = node.location().lon();
      record.tags = extract_tags(node.tags());

      auto [iter, inserted] = internal_.node_index_lookup.emplace(id, internal_.data.nodes.size());
      if (inserted) {
        internal_.data.nodes.emplace_back(record);
      }
    }

    if (auto poi_category = detect_poi_category(node.tags())) {
      PoiRecord poi;
      poi.osm_id = id;
      poi.lat = node.location().lat();
      poi.lon = node.location().lon();
      poi.category = std::move(*poi_category);
      poi.tags = extract_tags(node.tags());
      if (const char* name = node.tags().get_value_by_key("name")) {
        poi.name = name;
      }
      internal_.data.pois.emplace_back(std::move(poi));
    }
  }

 private:
  ConverterDataInternal& internal_;
};

class RelationCollector final : public osmium::handler::Handler {
 public:
  explicit RelationCollector(ConverterDataInternal& internal)
      : internal_(internal) {}

  void relation(const osmium::Relation& relation) {
    RelationRecord record;
    record.osm_id = relation.id();
    record.tags = extract_tags(relation.tags());

    // Extract members
    for (const auto& member : relation.members()) {
      record.member_ids.push_back(member.ref());
      
      // Convert osmium member type to our enum
      std::uint8_t member_type;
      switch (member.type()) {
        case osmium::item_type::node:
          member_type = 0; // Node
          break;
        case osmium::item_type::way:
          member_type = 1; // Way
          break;
        case osmium::item_type::relation:
          member_type = 2; // Relation
          break;
        default:
          continue; // Skip unknown types
      }
      record.member_types.push_back(member_type);
      
      // Store role
      if (member.role()) {
        record.member_roles.emplace_back(member.role());
      } else {
        record.member_roles.emplace_back(""); // Empty role
      }
    }

    internal_.data.relations.emplace_back(std::move(record));
  }

 private:
  ConverterDataInternal& internal_;
};

class WayNodeValidator final : public osmium::handler::Handler {
 public:
  explicit WayNodeValidator(ConverterDataInternal& internal)
      : internal_(internal) {}

  void way(const osmium::Way& way) {
  if (!way.tags().has_key("highway")) {
      return;
    }

    for (const auto& node_ref : way.nodes()) {
      if (!internal_.node_index_lookup.contains(node_ref.ref())) {
        internal_.missing_node_ids.push_back(node_ref.ref());
      }
    }
  }

 private:
  ConverterDataInternal& internal_;
};

template <typename T>
void write_pod(std::ofstream& out, const T& value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void write_string(std::ofstream& out, const std::string& value) {
  const std::uint32_t length = static_cast<std::uint32_t>(value.size());
  write_pod(out, length);
  out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

void write_node_refs(std::ofstream& out, const std::vector<std::int64_t>& refs) {
  const std::uint32_t count = static_cast<std::uint32_t>(refs.size());
  write_pod(out, count);
  if (!refs.empty()) {
    out.write(reinterpret_cast<const char*>(refs.data()),
              static_cast<std::streamsize>(refs.size() * sizeof(std::int64_t)));
  }
}

void write_tags(std::ofstream& out, const std::vector<std::pair<std::string, std::string>>& tags) {
  const std::uint32_t tag_count = static_cast<std::uint32_t>(tags.size());
  write_pod(out, tag_count);
  
  for (const auto& tag : tags) {
    write_string(out, tag.first);  // key
    write_string(out, tag.second); // value
  }
}

void write_streets_file(const ConverterDataInternal& internal, const fs::path& output_file) {
  std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("Failed to open streets output file: " + output_file.string());
  }

  out.write(kStreetsMagic, sizeof(kStreetsMagic));
  write_pod(out, kSchemaVersion);

  const std::uint64_t node_count = internal.data.nodes.size();
  const std::uint64_t segment_count = internal.data.street_segments.size();
  write_pod(out, node_count);
  write_pod(out, segment_count);

  for (const auto& node : internal.data.nodes) {
    write_pod(out, node.osm_id);
    write_pod(out, node.lat);
    write_pod(out, node.lon);
    write_tags(out, node.tags);
  }

  for (const auto& segment : internal.data.street_segments) {
    write_pod(out, segment.osm_id);
    const std::uint8_t category = static_cast<std::uint8_t>(segment.category);
    write_pod(out, category);
    write_pod(out, segment.max_speed_kph);
    write_string(out, segment.name);
    write_node_refs(out, segment.node_refs);
    write_tags(out, segment.tags);
  }
}

void write_osm_file(const ConverterDataInternal& internal, const fs::path& output_file) {
  std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("Failed to open OSM output file: " + output_file.string());
  }

  out.write(kOsmMagic, sizeof(kOsmMagic));
  write_pod(out, kSchemaVersion);

  const std::uint64_t poi_count = internal.data.pois.size();
  write_pod(out, poi_count);

  for (const auto& poi : internal.data.pois) {
    write_pod(out, poi.osm_id);
    write_pod(out, poi.lat);
    write_pod(out, poi.lon);
    write_string(out, poi.category);
    write_string(out, poi.name);
    write_tags(out, poi.tags);
  }

  const std::uint64_t feature_count = internal.data.features.size();
  write_pod(out, feature_count);

  for (const auto& feature : internal.data.features) {
    write_pod(out, feature.osm_id);
    write_pod(out, static_cast<std::uint8_t>(feature.type));
    write_string(out, feature.name);
    write_pod(out, static_cast<std::uint64_t>(feature.node_refs.size()));
    for (const auto& node_ref : feature.node_refs) {
      write_pod(out, node_ref);
    }
    write_pod(out, feature.is_closed);
    write_tags(out, feature.tags);
  }

  const std::uint64_t relation_count = internal.data.relations.size();
  write_pod(out, relation_count);

  for (const auto& relation : internal.data.relations) {
    write_pod(out, relation.osm_id);
    write_tags(out, relation.tags);
    
    const std::uint32_t member_count = static_cast<std::uint32_t>(relation.member_ids.size());
    write_pod(out, member_count);
    
    for (std::size_t i = 0; i < relation.member_ids.size(); ++i) {
      write_pod(out, relation.member_ids[i]);
      write_pod(out, relation.member_types[i]);
      write_string(out, relation.member_roles[i]);
    }
  }
}

ConverterDataInternal build_dataset(const fs::path& input, bool quiet) {
  ConverterDataInternal internal;

  {
    osmium::io::Reader way_reader{input, osmium::osm_entity_bits::way};
    HighwayCollector highway_handler{internal};
    FeatureCollector feature_handler{internal};
    osmium::apply(way_reader, highway_handler, feature_handler);
    way_reader.close();
  }

  {
    osmium::io::Reader node_reader{input, osmium::osm_entity_bits::node};
    NodeCollector node_handler{internal};
    osmium::apply(node_reader, node_handler);
    node_reader.close();
  }

  {
    osmium::io::Reader relation_reader{input, osmium::osm_entity_bits::relation};
    RelationCollector relation_handler{internal};
    osmium::apply(relation_reader, relation_handler);
    relation_reader.close();
  }

  {
    osmium::io::Reader validator_reader{input, osmium::osm_entity_bits::way};
    WayNodeValidator validator{internal};
    osmium::apply(validator_reader, validator);
    validator_reader.close();
  }

  if (!internal.missing_node_ids.empty() && !quiet) {
    std::sort(internal.missing_node_ids.begin(), internal.missing_node_ids.end());
    internal.missing_node_ids.erase(
        std::unique(internal.missing_node_ids.begin(), internal.missing_node_ids.end()),
        internal.missing_node_ids.end());
    std::cerr << "Warning: missing " << internal.missing_node_ids.size()
              << " node locations referenced by highway ways." << std::endl;
  }

  return internal;
}

}  // namespace

int run_converter(const ConverterConfig& config) {
  if (config.input_pbf.empty()) {
    std::cerr << "[converter] Missing --input argument" << std::endl;
    return 1;
  }

  if (!fs::exists(config.input_pbf)) {
    std::cerr << "[converter] Input file does not exist: " << config.input_pbf << std::endl;
    return 1;
  }

  std::string map_name = config.map_name;
  if (map_name.empty()) {
    map_name = config.input_pbf.stem().string();
  }

  fs::path output_dir = config.output_directory;
  if (output_dir.empty()) {
    output_dir = fs::current_path();
  }

  const fs::path streets_path = output_dir / (map_name + ".streets.bin");
  const fs::path osm_path = output_dir / (map_name + ".osm.bin");

  const bool both_exist = fs::exists(streets_path) && fs::exists(osm_path);
  if (both_exist && !config.force_rebuild) {
    if (!config.quiet) {
      std::cout << "[converter] Existing binaries found; skipping conversion for " << map_name
                << std::endl;
    }
    return 0;
  }

  try {
    if (!output_dir.empty()) {
      fs::create_directories(output_dir);
    }
  } catch (const std::exception& ex) {
    std::cerr << "[converter] Failed to create output directory: " << ex.what() << std::endl;
    return 1;
  }

  if (!config.quiet) {
    std::cout << "[converter] Converting " << config.input_pbf << " -> " << streets_path
              << " / " << osm_path << std::endl;
  }

  const auto start_time = std::chrono::steady_clock::now();

  ConverterDataInternal internal;
  try {
    internal = build_dataset(config.input_pbf, config.quiet);
    write_streets_file(internal, streets_path);
    write_osm_file(internal, osm_path);
  } catch (const std::exception& ex) {
    std::cerr << "[converter] Conversion failed: " << ex.what() << std::endl;
    return 1;
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time);

  if (!config.quiet) {
    std::cout << "[converter] Wrote " << internal.data.nodes.size() << " nodes, "
              << internal.data.street_segments.size() << " street segments, "
              << internal.data.pois.size() << " POIs, "
              << internal.data.features.size() << " features in " << elapsed.count() << "ms"
              << std::endl;
  }

  return 0;
}

}  // namespace gisevo::converter
