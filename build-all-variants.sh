#!/bin/bash
set -e

echo "============================================"
echo "Building WiCAN with MQTT Broker"
echo "All Hardware Variants"
echo "============================================"

# Get version from git
VERSION=$(git describe --tags --always --dirty)
echo "Version: $VERSION"

# Create releases directory
mkdir -p releases

# Hardware variants (update based on actual values in CMakeLists.txt)
# Check CMakeLists.txt for actual variable names
echo ""
echo "Building binaries for all variants..."
echo ""

# Just build current configuration
# (For multi-variant, you'd need to modify CMakeLists.txt for each build)
echo "Cleaning previous build..."
idf.py fullclean

echo "Building firmware..."
idf.py build

# Copy binary with version
BINARY_NAME="wican-broker-${VERSION}.bin"
if [ -f "build/wican-fw.bin" ]; then
    cp "build/wican-fw.bin" "releases/${BINARY_NAME}"
    echo "✅ Binary created: releases/${BINARY_NAME}"
else
    echo "❌ Build failed - binary not found"
    exit 1
fi

# Create merged binary for flashing (includes bootloader + partition table)
if [ -f "build/bootloader/bootloader.bin" ]; then
    MERGED_NAME="wican-broker-${VERSION}-merged.bin"
    echo "Creating merged binary with bootloader..."

    # Merge all necessary components
    # Offsets may vary - check build log for correct values
    esptool.py --chip esp32c3 merge_bin \
        -o "releases/${MERGED_NAME}" \
        --flash_mode dio \
        --flash_freq 80m \
        --flash_size 4MB \
        0x0 build/bootloader/bootloader.bin \
        0x8000 build/partition_table/partition-table.bin \
        0x10000 build/wican-fw.bin \
        2>/dev/null || echo "⚠️  esptool merge failed - install with: pip install esptool"

    if [ -f "releases/${MERGED_NAME}" ]; then
        echo "✅ Merged binary created: releases/${MERGED_NAME}"
    fi
fi

echo ""
echo "============================================"
echo "Build Complete!"
echo "============================================"
echo "Binaries in releases/ folder:"
ls -lh releases/
echo ""
echo "To flash:"
echo "  esptool.py --chip esp32c3 write_flash 0x0 releases/${BINARY_NAME}"
echo ""
