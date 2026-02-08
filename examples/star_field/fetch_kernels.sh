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
mkdir -p "$OUTPUT_DIR"/{fk,sclk,ck,spk}

# Get the frame kernel
wget -P "$OUTPUT_DIR/fk/" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/fk/orx_v14.tf

# Get the Spacecraft Clock kernel
wget -P "$OUTPUT_DIR/sclk/" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/sclk/orx_sclkscet_00093.tsc

# Get the CK kernels
wget -P "$OUTPUT_DIR/ck/" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/ck/orx_struct_mapcam_v01.bc
wget -P "$OUTPUT_DIR/ck/" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/ck/orx_sc_rel_160919_160925_v01.bc

# Get the SPK kernels
wget -P "$OUTPUT_DIR/spk/" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/spk/orx_160909_171201_170830_od023_v1.bsp
wget -P "$OUTPUT_DIR/spk/" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/spk/orx_struct_v04.bsp
wget -P "$OUTPUT_DIR/spk/" https://naif.jpl.nasa.gov/pub/naif/ORX/kernels/spk/de424.bsp

echo "Download complete! Files saved to: $OUTPUT_DIR"
