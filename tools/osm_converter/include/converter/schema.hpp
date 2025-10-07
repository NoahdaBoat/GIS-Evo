#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gisevo::converter {

inline constexpr std::uint32_t kSchemaVersion = 1;
inline constexpr char kStreetsMagic[8] = {'G', 'I', 'S', 'E', 'V', 'O', 'S', '1'};
inline constexpr char kOsmMagic[8] = {'G', 'I', 'S', 'E', 'V', 'O', 'O', '1'};

struct NodeRecord {
  std::int64_t osm_id;
  double lat;
  double lon;
};

enum class HighwayCategory : std::uint8_t {
  kUnknown = 0,
  kMotorway,
  kTrunk,
  kPrimary,
  kSecondary,
  kTertiary,
  kResidential,
  kService,
  kTrack,
  kFootway,
  kPath,
  kCycleway,
};

struct StreetSegmentRecord {
  std::int64_t osm_id;
  HighwayCategory category;
  float max_speed_kph;
  std::string name;
  std::vector<std::int64_t> node_refs;
};

struct PoiRecord {
  std::int64_t osm_id;
  double lat;
  double lon;
  std::string category;
  std::string name;
};

struct ConverterData {
  std::vector<NodeRecord> nodes;
  std::vector<StreetSegmentRecord> street_segments;
  std::vector<PoiRecord> pois;
};

}  // namespace gisevo::converter
