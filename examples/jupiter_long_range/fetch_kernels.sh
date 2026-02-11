#!/bin/bash

# Check if directory argument is provided
if [ $# -eq 0 ]; then
    echo "Error: No directory specified"
    echo "Usage: $0 <output_directory>"
    echo "Example: $0 C:/Users/chris/huira_data/kernels/"
    exit 1
fi

# Get the output directory from the first argument
OUTPUT_DIR="$1"

# Remove trailing slash if present
OUTPUT_DIR="${OUTPUT_DIR%/}"

# Create the base directory and subdirectories
mkdir -p "$OUTPUT_DIR"/{spk}

# Get the SPK kernels
wget -P "$OUTPUT_DIR/spk/" https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp
wget -P "$OUTPUT_DIR/spk/" https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/jup365.bsp

echo "Download complete! Files saved to: $OUTPUT_DIR"
