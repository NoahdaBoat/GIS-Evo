# GIS Evo OSM Converter

This utility turns raw OpenStreetMap `.osm.pbf` extracts into the
`*.streets.bin` and `*.osm.bin` binaries consumed by GIS Evo. It is
implemented in C++20 on top of the header-only `libosmium` and
`protozero` libraries to keep the conversion toolchain in the same
language as the runtime renderer.

## Building

Install the development headers for GTK (required by the viewer) and
libosmium:

```bash
sudo apt install libgtk-4-dev libosmium2-dev libprotozero-dev
```

Then build the project with Meson/Ninja:

```bash
meson setup build
meson compile -C build osm_converter
```

The resulting binary lives at `build/tools/osm_converter/osm_converter`.

## Usage

```bash
./build/tools/osm_converter/osm_converter \
  --input /path/to/map.osm.pbf \
  --output-dir ./resources/maps \
  --map-name toronto
```

By default the converter will skip work if both `toronto.streets.bin`
and `toronto.osm.bin` already exist. Use `--force` to regenerate them.

See the inline documentation in `converter.hpp`/`converter.cpp` for the
current on-disk schema.
