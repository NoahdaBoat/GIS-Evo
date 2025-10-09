#ifndef OSMDATABASEAPI_H
#define OSMDATABASEAPI_H

#include "StreetsDatabaseAPI.h"
#include <vector>

// TypedOSMID - OSM Entity with type information
class TypedOSMID {
public:
    enum EntityType {
        Node,
        Way,
        Relation,
        Invalid
    };
    
    TypedOSMID() : m_id(0), m_type(Invalid) {}
    TypedOSMID(OSMID id, EntityType type) : m_id(id), m_type(type) {}
    
    OSMID id() const { return m_id; }
    EntityType type() const { return m_type; }
    
private:
    OSMID m_id;
    EntityType m_type;
};

// FeatureType enumeration
enum class FeatureType {
    UNKNOWN,
    PARK,
    BEACH,
    LAKE,
    RIVER,
    ISLAND,
    BUILDING,
    GREENSPACE,
    GOLFCOURSE,
    STREAM
};

// OSMEntity base class
class OSMEntity {
public:
    virtual ~OSMEntity() = default;
    virtual OSMID id() const = 0;
};

// Function to get relation members as TypedOSMID
std::vector<TypedOSMID> getRelationMembers(const OSMRelation* relation);

// Function to get way members (node IDs)
std::vector<OSMID> getWayMembers(const OSMWay* way);

#endif // OSMDATABASEAPI_H