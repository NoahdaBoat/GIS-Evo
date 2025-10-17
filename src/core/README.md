# GIS Evo - Clean Architecture

This directory contains a completely new, modern architecture for the GIS Evo application that eliminates the problematic legacy m1-m4 code dependencies.

## Architecture Overview

### Core Components

1. **`map_data.hpp/cpp`** - Clean data abstraction layer
   - `MapData` class for loading and managing map data
   - Clean data structures (`StreetSegment`, `Intersection`, `POI`, `Feature`)
   - Spatial indexing with R-tree for efficient queries
   - No global state pollution

2. **`renderer.hpp/cpp`** - Modern rendering engine
   - `Renderer` class for drawing map elements
   - `CoordinateSystem` for proper coordinate transformations
   - `RenderStyle` system for customizable appearance
   - Clean separation of rendering logic

3. **`ui.hpp/cpp`** - GTK4 UI components
   - `MapView` widget with modern event handling
   - `Viewport` class for pan/zoom management
   - `Application` class for app lifecycle
   - No legacy UI dependencies

## Key Improvements

### ✅ Eliminated Problems
- **No more global state**: Removed `globals.h` dependency
- **Clean APIs**: Consistent, modern C++ interfaces
- **Proper separation**: Data, rendering, and UI are separate layers
- **Memory safety**: RAII and smart pointers throughout
- **Performance**: Efficient spatial indexing and viewport culling

### ✅ Modern C++ Features
- C++17 standard library usage
- RAII and smart pointers
- Namespace organization (`gisevo::core`, `gisevo::rendering`, `gisevo::ui`)
- Clean error handling
- Type safety

### ✅ Maintainable Design
- Single responsibility principle
- Dependency injection
- Interface-based design
- Easy to test and extend

## Building

```bash
cd src/core
meson setup build
meson compile -C build
```

## Usage

```cpp
#include "core/ui.hpp"

int main() {
    gisevo::ui::Application app;
    app.load_map("streets.bin", "osm.bin");
    return app.run(argc, argv);
}
```

## Migration Strategy

### Phase 1: Core Infrastructure ✅
- [x] Design clean architecture
- [x] Implement core data structures
- [x] Create rendering engine
- [x] Build GTK4 UI components

### Phase 2: Binary Loader Implementation
- [ ] Implement `MapData::load_from_binary()`
- [ ] Create binary format parser
- [ ] Add data validation

### Phase 3: Feature Parity
- [ ] Port all rendering features
- [ ] Add search functionality
- [ ] Implement pathfinding algorithms
- [ ] Add POI filtering

### Phase 4: Legacy Removal
- [ ] Remove m1-m4 dependencies
- [ ] Delete legacy code
- [ ] Update build system
- [ ] Clean up project structure

## Benefits

1. **Maintainability**: Clean, readable code that's easy to modify
2. **Performance**: Efficient spatial queries and rendering
3. **Extensibility**: Easy to add new features and data types
4. **Testing**: Each component can be unit tested independently
5. **Documentation**: Self-documenting code with clear interfaces

## Current Status

The new architecture is **structurally complete** but needs:
- Binary data loader implementation
- Integration with existing binary data files
- Testing and validation

This provides a solid foundation for a modern, maintainable GIS application without the technical debt of the legacy system.
