# WiCAN with MQTT Broker - Distribution Guide

## For End Users

### What is This?

This is a **modified version of WiCAN firmware** that includes a built-in MQTT broker. You can flash this instead of the official firmware to get local MQTT broker functionality.

### Installation (No Compilation Required!)

#### Option 1: Web Flasher (Easiest)

1. Go to: `https://YOUR-GITHUB-USERNAME.github.io/wican-fw-broker/` (you'll host this)
2. Connect WiCAN via USB
3. Click "Flash"
4. Done!

#### Option 2: Manual Flash using esptool

1. Download the `.bin` file for your hardware from [Releases](https://github.com/YOUR-USERNAME/wican-fw-broker/releases)
2. Install esptool: `pip install esptool`
3. Flash:
   ```bash
   esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x0 wican-broker-v1.0.bin
   ```

#### Option 3: ESP Flash Download Tool (Windows)

1. Download [Flash Download Tool](https://www.espressif.com/en/support/download/other-tools)
2. Load the `.bin` file
3. Set offset to `0x0`
4. Flash

### After Flashing

1. Device reboots and creates WiFi AP: `WiCAN_XXXXXX`
2. Connect to WiFi (password: `@meatpi#`)
3. Open browser to `http://192.168.80.1`
4. Enable MQTT Broker:
   - Set `mqtt_broker_en` to `enable`
   - Set `mqtt_en` to `enable` (for client to publish)
   - Save and reboot
5. Connect MQTT clients to `192.168.80.1:1883`

---

## For Maintainers (You)

### Build Process

```bash
# Clean build
idf.py fullclean

# Build for each hardware variant
idf.py build

# Output will be in build/ folder
```

### Release Workflow

1. **Track Upstream Changes**
   ```bash
   # Add official WiCAN as upstream
   git remote add upstream https://github.com/meatpiHQ/wican-fw.git

   # Fetch latest
   git fetch upstream

   # Merge (or cherry-pick specific commits)
   git merge upstream/main
   ```

2. **Resolve Conflicts** (if any) in:
   - `main/config_server.c`
   - `main/config_server.h`
   - `main/main.c`
   - `main/mqtt.c`

3. **Build Binaries**
   ```bash
   ./build-all-variants.sh
   ```

4. **Create Release**
   - Tag version: `git tag v1.0.0-broker`
   - Push: `git push origin v1.0.0-broker`
   - GitHub Actions auto-builds and creates release

5. **Users Download** pre-built binaries from GitHub Releases

### File Changes Summary

Keep this list for re-applying changes after upstream merges:

**Modified Files:**
- `main/idf_component.yml` - Added mosquitto dependency
- `main/config_server.h` - Added broker config fields + getter declarations
- `main/config_server.c` - Added broker getters, JSON handling, defaults
- `main/main.c` - Added mqtt_broker.h include + broker init code
- `main/mqtt.c` - Modified to connect to local broker when enabled

**New Files:**
- `main/mqtt_broker.c` - Broker implementation
- `main/mqtt_broker.h` - Broker API

**Config Changes:**
- Added `mqtt_broker_en` field
- Added `mqtt_broker_port` field
- Updated default config JSON

### Keeping Changes Minimal

The current changes are already pretty minimal. To make future merges easier:

1. **Keep broker code isolated** (mqtt_broker.c/h) ✅ Already done
2. **Minimize main.c changes** ✅ Just a few lines for init
3. **Config changes are isolated** ✅ Just adding new fields
4. **MQTT client changes are minimal** ✅ Just broker detection logic

### Automated Build Script

Create `build-all-variants.sh`:
```bash
#!/bin/bash
set -e

echo "Building WiCAN with MQTT Broker - All Variants"

# Array of hardware variants
variants=("WICAN_V210" "WICAN_V300" "WICAN_USB_V100" "WICAN_PRO")

mkdir -p releases

for variant in "${variants[@]}"; do
    echo "Building for $variant..."

    # Set hardware version in CMakeLists.txt
    sed -i "s/set(HARDWARE_VERSION .*/set(HARDWARE_VERSION $variant)/" CMakeLists.txt

    # Clean and build
    idf.py fullclean
    idf.py build

    # Copy binary to releases
    cp build/wican-fw.bin "releases/wican-broker-${variant}-$(git describe --tags).bin"
done

echo "All builds complete! Check releases/ folder"
```

### GitHub Actions CI/CD

Create `.github/workflows/release.yml`:
```yaml
name: Build and Release

on:
  push:
    tags:
      - 'v*-broker'

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        variant: [WICAN_V210, WICAN_V300, WICAN_USB_V100, WICAN_PRO]

    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive

      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.4.1

      - name: Build firmware
        run: |
          . $IDF_PATH/export.sh
          idf.py build

      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: wican-broker-${{ matrix.variant }}
          path: build/*.bin

  release:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/download-artifact@v3
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: '**/*.bin'
          body: |
            WiCAN Firmware with MQTT Broker

            Flash this firmware to add local MQTT broker capability.
            See MQTT_BROKER_README.md for usage instructions.
```

---

## Comparison to Official Firmware

| Feature | Official WiCAN | WiCAN + Broker |
|---------|----------------|----------------|
| MQTT Client | ✅ | ✅ |
| External Broker Support | ✅ | ✅ |
| **Local MQTT Broker** | ❌ | ✅ |
| CAN Bus Interface | ✅ | ✅ |
| WiFi AP/STA | ✅ | ✅ |
| Web Config | ✅ | ✅ |
| BLE | ✅ | ✅ |
| Flash Size | ~1.2 MB | ~1.26 MB (+60KB) |

---

## Support

- **For broker-specific issues**: [Your repo]/issues
- **For general WiCAN issues**: [Official WiCAN repo]/issues
- **Upstream**: Based on official WiCAN firmware by Meatpi Electronics

## License

Maintains GPL-3.0 license from original WiCAN project.
