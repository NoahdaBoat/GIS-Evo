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

    TypedOSMID& operator=(OSMID osmid) { m_id = osmid; return *this; }

    OSMID id() const { return m_id; }
    EntityType type() const { return m_type; }
    
    // Comparison operators
    bool operator==(OSMID osmid) const { return m_id == osmid; }
    bool operator==(const TypedOSMID& other) const { return m_id == other.m_id && m_type == other.m_type; }
    
private:
    OSMID m_id;
    EntityType m_type;
};

// OSMEntity base class
class OSMEntity {
public:
    virtual ~OSMEntity() = default;
    virtual OSMID id() const = 0;
};

// Function to get relation members as TypedOSMID
std::vector<TypedOSMID> getRelationMembers(const OSMRelation* relation);

// Stubbed OSM helpers for path-finding (Task C9)
std::vector<OSMID> getWayNodes(OSMID way_id);
TypedOSMID getNodeByOSMID(OSMID node_id);

// Function to get way members (node IDs)
std::vector<OSMID> getWayMembers(const OSMWay* way);

// Missing functions for compilation
bool loadOSMDatabaseBIN(std::string path);
void closeOSMDatabase();

#endif // OSMDATABASEAPI_H