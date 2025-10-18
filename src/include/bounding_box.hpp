#pragma once

#include <algorithm>

namespace gisevo {

struct BoundingBox {
    double min_x;
    double min_y;
    double max_x;
    double max_y;

    BoundingBox()
        : min_x(0.0)
        , min_y(0.0)
        , max_x(0.0)
        , max_y(0.0) {}

    BoundingBox(double min_x_in, double min_y_in, double max_x_in, double max_y_in)
        : min_x(min_x_in)
        , min_y(min_y_in)
        , max_x(max_x_in)
        , max_y(max_y_in) {}

    [[nodiscard]] bool intersects(const BoundingBox& other) const {
        return !(max_x < other.min_x || min_x > other.max_x ||
                 max_y < other.min_y || min_y > other.max_y);
    }

    [[nodiscard]] bool contains(double x, double y) const {
        return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
    }

    void expand(const BoundingBox& other) {
        min_x = std::min(min_x, other.min_x);
        min_y = std::min(min_y, other.min_y);
        max_x = std::max(max_x, other.max_x);
        max_y = std::max(max_y, other.max_y);
    }

    [[nodiscard]] double area() const {
        const double width = max_x - min_x;
        const double height = max_y - min_y;
        if (width <= 0.0 || height <= 0.0) {
            return 0.0;
        }
        return width * height;
    }

    [[nodiscard]] double perimeter() const {
        const double width = max_x - min_x;
        const double height = max_y - min_y;
        if (width <= 0.0 || height <= 0.0) {
            return 0.0;
        }
        return 2.0 * (width + height);
    }
};

} // namespace gisevo
