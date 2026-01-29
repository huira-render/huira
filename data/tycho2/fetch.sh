#!/bin/bash
set -euo pipefail

# Download Tycho-2 star catalog data files from CDS Strasbourg

if [[ -z "${1:-}" ]]; then
    echo "Usage: $0 <output_directory>"
    echo "Example: $0 /home/user/huira_data/"
    exit 1
fi

OUTPUT_DIR="${1%/}/tycho2"
BASE_URL="https://cdsarc.cds.unistra.fr/viz-bin/nph-Cat/txt?I/259"

mkdir -p "$OUTPUT_DIR"

for i in $(seq -w 0 19); do
    file="tyc2.dat.$i"
    echo "Downloading $file..."
    curl -fsSL "${BASE_URL}/${file}.gz" -o "$OUTPUT_DIR/$file"
done

echo "Downloaded to: $OUTPUT_DIR"
