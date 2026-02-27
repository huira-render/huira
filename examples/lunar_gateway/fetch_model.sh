#!/bin/bash

# Check if directory argument is provided
if [ $# -eq 0 ]; then
    echo "Error: No directory specified"
    echo "Usage: $0 <output_directory>"
    echo "Example: $0 C:/Users/chris/huira_data/models/"
    exit 1
fi

# Get the output directory from the first argument
OUTPUT_DIR="$1"

# Remove trailing slash if present
OUTPUT_DIR="${OUTPUT_DIR%/}"

# Create the directory
mkdir -p "$OUTPUT_DIR"

# Download the Gateway model
wget -O "$OUTPUT_DIR/gateway.glb" "https://assets.science.nasa.gov/content/dam/science/cds/3d/resources/model/gateway/Gateway%20Core.glb"

echo "Download complete! File saved to: $OUTPUT_DIR/gateway.glb"
