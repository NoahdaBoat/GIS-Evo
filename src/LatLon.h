#ifndef LATLON_H
#define LATLON_H

class LatLon {
public:
    LatLon(double lat = 0.0, double lon = 0.0) : lat_(lat), lon_(lon) {}
    double latitude() const { return lat_; }
    double longitude() const { return lon_; }
    bool operator==(const LatLon& other) const {
        return lat_ == other.lat_ && lon_ == other.lon_;
    }
private:
    double lat_;
    double lon_;
};

#endif // LATLON_H