#ifndef M2_H
#define M2_H

#include <string>
#include <vector>
#include "LatLon.h"

// Forward declarations for GTK types to avoid including heavy GTK headers
typedef struct _GtkApplication GtkApplication;
typedef struct _GtkEntry GtkEntry;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkToggleButton GtkToggleButton;
typedef struct _GdkEventButton GdkEventButton;
typedef struct _cairo cairo_t;

// Type definitions
using IntersectionIdx = unsigned;
using POIIdx = unsigned;
using StreetSegmentIdx = unsigned;

// Point2D structure for coordinate handling
struct Point2D {
    double x;
    double y;
};

// Rectangle structure for visible world bounds
struct Rectangle {
    double left;
    double top;
    double right;
    double bottom;
};

// ==================== Main Entry Point ====================
// Main function to initialize and display the map using GTK4
void drawMap();

// ==================== GTK4 Setup and Initialization ====================
// Initial setup of UI components and callbacks (legacy EZGL compatibility)
void initial_setup(GtkApplication* application, bool new_window);

// ==================== Drawing Functions ====================
// Main canvas drawing function called by GTK4 draw callback
void draw_main_canvas(cairo_t* cr, int width, int height);

// Draw street network on the canvas
void drawStreets(cairo_t* cr);

// Draw natural and built features (water, parks, buildings, etc.)
void draw_features(cairo_t* cr);

// Draw OSM way-based features
void way_draw_features(cairo_t* cr);

// Draw Points of Interest (POIs) as icons
void drawPOIPng(cairo_t* cr);

// Draw highlighted intersections (selected by user)
void drawHighlightedIntersections(cairo_t* cr);

// Helper to determine if an object should be drawn at current zoom level
bool shouldDrawAtZoomLevel(int object_zoom_threshold);

// ==================== User Interaction Callbacks ====================
// Clear all highlighted intersections and routes
void clearAllHighlights(GtkApplication* application);

// Handle search bar Enter key press
void searchEntryEnter(GtkEntry* search_bar, GtkApplication* application);

// Handle search bar text changes (for autocomplete)
void searchEntryType(GtkEntry* search_bar, GtkApplication* application);

// Handle mouse click events on the map canvas
void actOnMouseClick(GtkApplication* application, GdkEventButton* event, double x, double y);

// Display route information and directions
void outputRoad(GtkApplication* application);

// Toggle search route mode on/off
void searchRouteToggle(GtkToggleButton* search_route_toggle, GtkApplication* application);

// ==================== Map Management ====================
// Load a new map (city) into the application
void loadNewMap(const std::string& new_city, GtkApplication* application);

// ==================== UI Helper Functions ====================
// Update status bar message
void updateStatusMessage(const std::string& message);

// ==================== Button Callbacks ====================
// Zoom fit button callback
void zoomFit(GtkEntry* zoom_fit_button, GtkApplication* application);

// Change map/city dropdown callback
void change_map(GtkEntry* city_maps, GtkApplication* application);

// Toggle entertainment POI visibility
void draw_ent(GtkEntry* ent_button, GtkApplication* application);

// Toggle transportation POI visibility
void draw_trans(GtkEntry* trans_button, GtkApplication* application);

// Toggle basic services POI visibility
void draw_basic(GtkEntry* basic_button, GtkApplication* application);

// Toggle dark mode
void darkMode(GtkEntry* dark_mode_button, GtkApplication* application);

// About button callback
void aboutButton(GtkWidget* about_button, GtkApplication* application);

// Help button callback
void helpButton(GtkWidget* help_button, GtkApplication* application);

// Dialog input handler
void dialogInput(GtkWidget* dialog, GtkApplication* application, void* input);

#endif // M2_H