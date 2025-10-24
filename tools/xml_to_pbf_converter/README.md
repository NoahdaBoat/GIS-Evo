# XML to PBF Converter

This utility converts OpenStreetMap XML files to the Protocolbuffer Binary Format (PBF) using libosmium. It's designed to efficiently convert large OSM XML files to the more compact and faster-to-process PBF format.

## Overview

The XML to PBF converter is a C++ tool built on top of libosmium that:
- Reads OpenStreetMap XML files (.osm, .xml)
- Converts them to PBF format (.osm.pbf)
- Preserves all OSM data including nodes, ways, relations, and metadata
- Provides significant file size reduction (typically 60-80% smaller)

## Building

### Prerequisites

Install the required dependencies:

```bash
# On Ubuntu/Debian
sudo apt install libosmium2-dev libprotozero-dev libexpat1-dev libbz2-dev

# On Fedora/RHEL
sudo dnf install libosmium-devel protozero-devel expat-devel bzip2-devel
```

### Build Instructions

```bash
# Configure and build the project
meson setup build
meson compile -C build xml_to_pbf_converter
```

The resulting binary will be located at:
```
build/tools/xml_to_pbf_converter/xml_to_pbf_converter
```

## Usage

### Basic Usage

```bash
./build/tools/xml_to_pbf_converter/xml_to_pbf_converter \
  --input input_file.xml \
  --output output_file.osm.pbf
```

### Command Line Options

| Option | Short | Description |
|--------|-------|-------------|
| `--input` | `-i` | Path to the source XML file (required) |
| `--output` | `-o` | Path for the output PBF file (required) |
| `--quiet` | `-q` | Suppress progress logging |
| `--help` | `-h` | Show help text |

### Examples

#### Convert a standard OSM XML file
```bash
./build/tools/xml_to_pbf_converter/xml_to_pbf_converter \
  --input toronto.osm \
  --output toronto.osm.pbf
```

#### Convert with quiet output
```bash
./build/tools/xml_to_pbf_converter/xml_to_pbf_converter \
  --input large_map.xml \
  --output large_map.osm.pbf \
  --quiet
```

#### Convert using absolute paths
```bash
./build/tools/xml_to_pbf_converter/xml_to_pbf_converter \
  --input /path/to/input/map.xml \
  --output /path/to/output/map.osm.pbf
```

## File Format Support

### Input Formats
- **OpenStreetMap XML** (.osm, .xml)
- **Compressed XML** (.osm.bz2, .xml.bz2) - automatically detected
- **Standard OSM XML format** with nodes, ways, and relations

### Output Format
- **Protocolbuffer Binary Format** (.osm.pbf)
- Compatible with all standard OSM tools and libraries
- Significantly smaller file size than XML
- Faster to read and process

## Performance Characteristics

### File Size Reduction
Typical compression ratios:
- **Small files** (< 10MB): 60-70% reduction
- **Medium files** (10-100MB): 70-80% reduction  
- **Large files** (> 100MB): 75-85% reduction

### Processing Speed
- **Memory efficient**: Processes files in chunks
- **Multi-threaded**: Uses libosmium's built-in threading
- **Streaming**: No need to load entire file into memory

## Error Handling

The converter provides clear error messages for common issues:

### File Not Found
```
[xml_to_pbf_converter] Input file does not exist: input_file.xml
```

### Format Detection Issues
```
[xml_to_pbf_converter] Conversion failed: Could not detect file format for filename 'file'
```
**Solution**: Ensure your input file has a proper extension (.osm, .xml)

### Permission Issues
```
[xml_to_pbf_converter] Failed to create output directory: filesystem error: permission denied
```
**Solution**: Check write permissions for the output directory

## Integration with GIS-Evo

This converter is designed to work seamlessly with the GIS-Evo project:

### Workflow Integration
1. **Convert XML to PBF**: Use this tool to convert raw OSM XML files
2. **Convert PBF to Binary**: Use the existing `osm_converter` tool to create GIS-Evo's custom binary format

```bash
# Step 1: Convert XML to PBF
./build/tools/xml_to_pbf_converter/xml_to_pbf_converter \
  --input raw_data.osm \
  --output processed.osm.pbf

# Step 2: Convert PBF to GIS-Evo binary format
./build/tools/osm_converter/osm_converter \
  --input processed.osm.pbf \
  --output-dir ./resources/maps \
  --map-name processed
```

## Technical Details

### Dependencies
- **libosmium**: Header-only C++ library for OSM data processing
- **protozero**: Protocol buffer parsing library
- **expat**: XML parsing library
- **zlib**: Compression library
- **bzip2**: Additional compression support

### Build Configuration
The tool is configured with the following compile flags:
- `DOSMIUM_WITH_XML_INPUT`: Enable XML input support
- `DOSMIUM_WITH_PBF_OUTPUT`: Enable PBF output support
- `DOSMIUM_WITH_PROTOZERO`: Enable protocol buffer support

### Memory Usage
- **Streaming processing**: Files are processed in chunks
- **Configurable buffer sizes**: Uses libosmium's default buffer management
- **Low memory footprint**: Suitable for large file processing

## Troubleshooting

### Common Issues

#### Build Failures
**Problem**: Missing dependencies
```bash
ERROR: Dependency "expat" not found
```
**Solution**: Install missing development packages as listed in prerequisites

#### Conversion Failures
**Problem**: Invalid XML format
```bash
[xml_to_pbf_converter] Conversion failed: XML parsing error
```
**Solution**: Verify your input file is valid OSM XML format

#### Large File Processing
**Problem**: Very large files (> 1GB) may take significant time
**Solution**: Use `--quiet` flag and be patient; the tool is memory-efficient

### Performance Tips

1. **Use SSD storage** for faster I/O operations
2. **Ensure sufficient disk space** (PBF files are smaller but still need space)
3. **Close other applications** when processing very large files
4. **Use absolute paths** to avoid path resolution issues

## License

This tool is part of the GIS-Evo project and follows the same licensing terms.

## Contributing

To contribute to this tool:
1. Follow the existing code style
2. Add appropriate error handling
3. Update this documentation for new features
4. Test with various file sizes and formats

## See Also

- [GIS-Evo OSM Converter](../osm_converter/README.md) - For PBF to binary conversion
- [libosmium Documentation](https://osmcode.org/libosmium/) - Underlying library documentation
- [OpenStreetMap PBF Format](https://wiki.openstreetmap.org/wiki/PBF_Format) - PBF format specification
