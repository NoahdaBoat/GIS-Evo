#ifndef GTK4_TYPES_HPP
#define GTK4_TYPES_HPP

#include <gtk/gtk.h>
#include <cairo.h>

// Custom Point2D struct to replace ezgl::point2d
struct Point2D {
    double x, y;
    
    Point2D() : x(0.0), y(0.0) {}
    Point2D(double x_, double y_) : x(x_), y(y_) {}
};

// Custom Rectangle struct to replace ezgl::rectangle
struct Rectangle {
    double x1, y1, x2, y2;
    
    Rectangle() : x1(0.0), y1(0.0), x2(0.0), y2(0.0) {}
    Rectangle(double x1_, double y1_, double x2_, double y2_) 
        : x1(x1_), y1(y1_), x2(x2_), y2(y2_) {}
    
    double left() const { return x1; }
    double right() const { return x2; }
    double bottom() const { return y1; }
    double top() const { return y2; }
    double width() const { return x2 - x1; }
    double height() const { return y2 - y1; }
    double center_x() const { return (x1 + x2) / 2.0; }
    double center_y() const { return (y1 + y2) / 2.0; }
    
    bool contains(const Point2D& point) const {
        return point.x >= x1 && point.x <= x2 && 
               point.y >= y1 && point.y <= y2;
    }
};

#endif // GTK4_TYPES_HPP
