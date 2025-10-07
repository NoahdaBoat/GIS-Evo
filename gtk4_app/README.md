# GIS Evo GTK4 Shell

This directory contains a minimal GTK4 application scaffold that will replace the deprecated `ezgl` wrapper. It provides:

- A CMake-free Meson build definition linking against `gtk4`.
- A standalone `MapView` widget implemented with modern event controllers, gestures, and drawing-area APIs.
- Keyboard and mouse interactions that mimic the original pan/zoom behaviour, ready to host the map rendering pipeline.

## Building

The project uses Meson and Ninja. Make sure the GTK4 development packages (`libgtk-4-dev` on Debian/Ubuntu) are installed.

```bash
meson setup build
meson compile -C build
meson install -C build  # optional
```

Run the preview application from the build folder:

```bash
./build/gis-evo-gtk4
```

## Next steps

- Port the OpenStreetMap rendering pipeline into `MapView::draw`.
- Replace hard-coded demo visuals with real layer drawing.
- Gradually migrate UI widgets from `ms2helpers`/`m2.cpp` to native GTK4 equivalents.