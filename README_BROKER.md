# WiCAN Firmware with Local MQTT Broker

> **A modified version of [WiCAN firmware](https://github.com/meatpiHQ/wican-fw) that adds a local MQTT broker**

## What's This?

This is the official WiCAN firmware **plus** a lightweight MQTT broker that runs directly on the ESP32 device. This means:

- ✅ **No cloud required** - broker runs on the device itself
- ✅ **No internet needed** - works completely offline
- ✅ **Lower latency** - direct WiFi connection, no round trip to cloud
- ✅ **Multiple subscribers** - multiple devices can connect locally
- ✅ **Better privacy** - vehicle data stays in your car
- ✅ **Standard MQTT** - uses Eclipse Mosquitto broker

## Quick Start for Users

### 1. Download & Flash

**Download:** [Latest Release](https://github.com/YOUR-USERNAME/wican-fw-broker/releases)

**Flash (easy method):**
```bash
pip install esptool
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x0 wican-broker-vX.X.X-merged.bin
```

See [QUICK_START.md](./QUICK_START.md) for detailed instructions.

### 2. Enable Broker

1. Connect to `WiCAN_XXXXXX` WiFi (password: `@meatpi#`)
2. Browser to `http://192.168.80.1`
3. Set `mqtt_broker_en` to `enable`
4. Set `mqtt_en` to `enable`
5. Save & reboot

### 3. Connect Apps

Any MQTT client can now connect to `192.168.80.1:1883` and subscribe to vehicle data!

**Example:**
```bash
mosquitto_sub -h 192.168.80.1 -t "wican/+/can/tx" -v
```

See [MQTT_BROKER_README.md](./MQTT_BROKER_README.md) for full documentation.

---

## For Developers

### Build from Source

```bash
git clone https://github.com/YOUR-USERNAME/wican-fw-broker.git
cd wican-fw-broker

# Setup ESP-IDF (v5.0+)
. ~/esp/esp-idf/export.sh

# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

### Build Release Binaries

```bash
./build-all-variants.sh
```

Output in `releases/` folder.

---

## Differences from Official WiCAN

| Aspect | Official WiCAN | This Fork |
|--------|----------------|-----------|
| **Firmware Size** | ~1.2 MB | ~1.26 MB (+60KB) |
| **RAM Usage** | Base | +6KB (broker task) |
| **MQTT Client** | ✅ External only | ✅ External or local |
| **MQTT Broker** | ❌ | ✅ Built-in |
| **Requires Internet** | For MQTT features | No (can work offline) |
| **All Other Features** | ✅ | ✅ Identical |

Everything else (CAN bus, BLE, TCP, web config, etc.) is **exactly the same** as official firmware!

---

## Files Changed

**Modified:**
- `main/idf_component.yml` - Added `espressif/mosquitto` dependency
- `main/config_server.h` - Added broker config fields + getters
- `main/config_server.c` - Added broker config parsing and API
- `main/main.c` - Added broker initialization (10 lines)
- `main/mqtt.c` - Auto-detect local broker and connect to it

**New:**
- `main/mqtt_broker.c` - Broker implementation (~100 lines)
- `main/mqtt_broker.h` - Broker API

**Documentation:**
- `MQTT_BROKER_README.md` - Full usage guide
- `QUICK_START.md` - User installation guide
- `BROKER_VARIANT.md` - Maintainer guide
- `.github/workflows/build-release.yml` - Auto-build on release

---

## Maintaining This Fork

### Syncing with Upstream

```bash
# Setup (first time)
git remote add upstream https://github.com/meatpiHQ/wican-fw.git

# Update
git fetch upstream
git merge upstream/main

# Resolve conflicts if any
# Test build
idf.py build

# Release
git tag v1.2.0-broker
git push origin v1.2.0-broker
```

GitHub Actions will automatically build binaries when you push a tag!

---

## Architecture

```
┌────────────────────────────────────────────┐
│            WiCAN ESP32 Device              │
│                                            │
│  ┌──────────┐     ┌──────────────────┐    │
│  │ CAN Bus  │────►│  MQTT Client     │    │
│  └──────────┘     │  (publishes to   │    │
│                   │   local broker)  │    │
│                   └────────┬─────────┘    │
│                            │               │
│                            ▼               │
│                   ┌──────────────────┐    │
│                   │  MQTT Broker     │    │
│                   │  (Mosquitto)     │    │
│                   │  Port: 1883      │    │
│                   └────────┬─────────┘    │
│                            │               │
│                   ┌────────▼─────────┐    │
│                   │   WiFi AP        │    │
│                   │   192.168.80.1   │    │
│                   └────────┬─────────┘    │
└────────────────────────────┼──────────────┘
                             │
         ┌───────────────────┼────────────────┐
         │                   │                │
    ┌────▼────┐         ┌───▼────┐      ┌────▼────┐
    │  Phone  │         │ Tablet │      │ Laptop  │
    │  (MQTT) │         │ (MQTT) │      │  (MQTT) │
    └─────────┘         └────────┘      └─────────┘
```

---

## Memory Footprint

- **Flash**: +60KB (Mosquitto broker code)
- **RAM**: +2KB startup + ~4KB per connected MQTT client
- **Stack**: +6KB (broker FreeRTOS task)

Total overhead is minimal and works fine on ESP32-C3.

---

## Support & Contributing

- 📖 **Full Documentation**: [MQTT_BROKER_README.md](./MQTT_BROKER_README.md)
- 🚀 **Quick Start**: [QUICK_START.md](./QUICK_START.md)
- 🐛 **Issues**: [GitHub Issues](https://github.com/YOUR-USERNAME/wican-fw-broker/issues)
- 💡 **Discussions**: [GitHub Discussions](https://github.com/YOUR-USERNAME/wican-fw-broker/discussions)
- 🔧 **Upstream**: [Official WiCAN](https://github.com/meatpiHQ/wican-fw)

---

## License

Maintains **GPL-3.0** license from original WiCAN project by Meatpi Electronics.

---

## Credits

- **Original WiCAN Firmware**: [Meatpi Electronics](https://github.com/meatpiHQ)
- **Mosquitto MQTT Broker**: [Eclipse Foundation](https://mosquitto.org)
- **Mosquitto ESP-IDF Port**: [Espressif Systems](https://github.com/espressif/esp-protocols)

---

## Why This Fork Exists

The official WiCAN firmware is excellent but requires an external MQTT broker (usually cloud-based). This fork adds the **option** to run a broker locally on the device itself, enabling:

1. Fully offline operation
2. No dependency on external services
3. Lower latency for in-vehicle apps
4. Better privacy (data stays local)
5. Multi-client support over local WiFi

It's perfect for:
- Race cars / track vehicles (no internet)
- Privacy-conscious users
- Development/testing environments
- Vehicles with spotty connectivity
- DIY dashboards and data loggers
