# WiCAN Firmware with MQTT Broker - Complete Documentation

> Modified version of [WiCAN firmware](https://github.com/meatpiHQ/wican-fw) with local MQTT broker and vehicle auto-detection

## Table of Contents

1. [Quick Start](#quick-start)
2. [MQTT Broker Feature](#mqtt-broker-feature)
3. [Vehicle Auto-Detection](#vehicle-auto-detection)
4. [API Reference](#api-reference)
5. [Building from Source](#building-from-source)
6. [Troubleshooting](#troubleshooting)
7. [Maintaining Your Fork](#maintaining-your-fork)

---

## Quick Start

### For End Users

#### 1. Download & Flash Firmware

**Download:** [Latest Release](https://github.com/adam-weber/wican-fw-mqtt-broker/releases)

**Flash with esptool (Mac/Linux):**
```bash
pip install esptool
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x0 wican-broker-vX.X.X-merged.bin
```

**Flash on Windows:**
1. Download [ESP Flash Tool](https://www.espressif.com/en/support/download/other-tools)
2. Load the `.bin` file at offset `0x0`
3. Select COM port and click "START"

#### 2. Enable MQTT Broker

1. Connect to `WiCAN_XXXXXX` WiFi (password: `@meatpi#`)
2. Browser to `http://192.168.80.1`
3. In MQTT settings:
   - Set `mqtt_broker_en` to `enable`
   - Set `mqtt_en` to `enable`
   - Optionally change `mqtt_broker_port` (default: `1883`)
4. Save & reboot

#### 3. Connect Your Apps

Any MQTT client can connect to `192.168.80.1:1883`:

**Command line:**
```bash
mosquitto_sub -h 192.168.80.1 -t "wican/+/can/tx" -v
```

**Python:**
```python
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    print(f"{msg.topic}: {msg.payload.decode()}")

client = mqtt.Client()
client.on_message = on_message
client.connect("192.168.80.1", 1883, 60)
client.subscribe("wican/+/can/tx")
client.loop_forever()
```

---

## MQTT Broker Feature

### Overview

This firmware includes an **embedded MQTT broker** (Eclipse Mosquitto) running directly on the WiCAN ESP32 device.

### Benefits

- ✅ **No cloud required** - broker runs on the device
- ✅ **No internet needed** - works completely offline
- ✅ **Lower latency** - direct WiFi connection
- ✅ **Multiple subscribers** - multiple devices can connect
- ✅ **Better privacy** - vehicle data stays local
- ✅ **Standard MQTT** - compatible with any MQTT client

### Architecture

```
┌────────────────────────────────────────────┐
│            WiCAN ESP32 Device              │
│                                            │
│  ┌──────────┐     ┌──────────────────┐    │
│  │ CAN Bus  │────►│  MQTT Client     │    │
│  └──────────┘     │  (publishes to   │    │
│                   │   local broker)  │    │
│                   └────────┬─────────┘    │
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

### Default Topics

- `wican/{DEVICE_ID}/can/tx` - CAN frames from vehicle
- `wican/{DEVICE_ID}/can/rx` - CAN frames sent to vehicle
- `wican/{DEVICE_ID}/can/status` - Device status

### Configuration Options

| Parameter | Default | Description |
|-----------|---------|-------------|
| `mqtt_broker_en` | `disable` | Enable/disable local broker |
| `mqtt_broker_port` | `1883` | Port number (1-65535) |
| `mqtt_publish_mode` | `2` (Hybrid) | 0=Static, 1=Dynamic, 2=Hybrid |

### Publishing Modes

**Static Mode (0):**
- Publishes all signals from vehicle profile (legacy behavior)
- Backwards compatible

**Dynamic Mode (1):**
- Only publishes signals requested by connected clients
- Efficient for limited bandwidth

**Hybrid Mode (2) - Recommended:**
- Publishes vehicle profile signals + dynamically requested signals
- Best of both worlds

### Memory Requirements

- **Flash**: +60 KB
- **RAM**: +2 KB startup + ~4 KB per client
- **Stack**: +6 KB (broker task)

---

## Vehicle Auto-Detection

### Overview

Automatically detect your vehicle by scanning the CAN bus and matching against known fingerprints (similar to comma.ai openpilot).

**This feature runs automatically** when you plug in the OBD connector and turn on the ignition. No user interaction needed!

### How It Works

1. **Plug in OBD** - Connect to vehicle's OBD port
2. **Turn ignition on** - CAN bus becomes active
3. **Auto-trigger** - After 50 CAN messages (~1-2 seconds), detection starts automatically
4. **Scan CAN Bus** - Monitors which CAN addresses are active for 15 seconds
5. **Match Fingerprints** - Compares against database of known vehicles
6. **Calculate Confidence** - Ranks matches by confidence score
7. **Auto-Load Profile** - Automatically configures signals for your vehicle

**Runs once per boot** - Detection only triggers on first CAN activity after power-up.

### Supported Vehicles

Pre-loaded fingerprints for 10 vehicles:
- Tesla Model 3 & Model Y
- Honda Civic
- Toyota Camry
- Ford F-150
- BMW 3 Series
- Hyundai Ioniq 5
- Chevrolet Bolt EV
- Nissan Leaf
- Volkswagen ID.4

### Using Auto-Detection

**It's automatic!** Just plug in the OBD connector and turn on the ignition. The system will:
- Detect CAN activity
- Start fingerprinting automatically
- Identify your vehicle
- Load the correct profile

No button clicks or configuration needed.

#### Optional: Manual Trigger via API

You can also trigger detection manually if needed:

**Start detection:**
```bash
curl -X POST http://192.168.80.1/api/vehicle/detect
```

**Check status:**
```bash
curl http://192.168.80.1/api/vehicle/detect/status
```

**Get results:**
```bash
curl http://192.168.80.1/api/vehicle/detect/result
```

### Dynamic Signal Subscription

Once vehicle is detected, external devices can request specific signals:

**Subscribe to a signal:**
```python
# Publish to request signal
mqtt_client.publish("wican/subscribe/vehicle_speed", "1")

# Listen for data
mqtt_client.subscribe("wican/data/vehicle_speed")
```

**Unsubscribe:**
```python
mqtt_client.publish("wican/unsubscribe/vehicle_speed", "1")
```

### Topic Structure

```
wican/subscribe/{signal_name}      ← Request signal
wican/unsubscribe/{signal_name}    ← Stop signal
wican/data/{signal_name}           ← Receive data
wican/clients/{client_id}/lwt      ← Last Will (auto-cleanup)
wican/status/signals               ← Active signals list
```

---

## API Reference

### Vehicle Detection Endpoints

**POST `/api/vehicle/detect`**
- Start vehicle detection
- Returns: `{"status": "started"}` or error

**GET `/api/vehicle/detect/status`**
- Get detection progress
- Returns: Progress percentage, addresses seen

**GET `/api/vehicle/detect/result`**
- Get detection results
- Returns: Array of matches with confidence scores

### MQTT Broker Functions

```c
// Initialize broker
int mqtt_broker_init(uint16_t port);

// Stop broker
void mqtt_broker_stop();

// Check if running
bool mqtt_broker_is_running();

// Get client count
uint8_t mqtt_broker_get_client_count();
```

### Vehicle Detection Functions

```c
// Initialize detection
int vehicle_detect_init(void);

// Start detection (blocking)
int vehicle_detect_start(detection_result_t *result);

// Start detection (async)
int vehicle_detect_start_async(void);

// Get status
int vehicle_detect_get_status(detection_status_t *status);

// Get results
int vehicle_detect_get_result(detection_result_t *result);

// Load fingerprints
int vehicle_detect_load_fingerprints(const char *json_data);
```

---

## Building from Source

### Prerequisites

- ESP-IDF v5.0 or later
- Python 3.7+
- Git

### Build Steps

```bash
# Clone repo
git clone https://github.com/adam-weber/wican-fw-mqtt-broker.git
cd wican-fw-mqtt-broker

# Setup ESP-IDF
. ~/esp/esp-idf/export.sh

# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

### Files Modified

**Modified:**
- `main/idf_component.yml` - Added mosquitto dependency
- `main/CMakeLists.txt` - Added new source files
- `main/config_server.h` - Added broker/detection config
- `main/config_server.c` - Added HTTP endpoints
- `main/main.c` - Initialize broker and detection

**New:**
- `main/mqtt_broker.c/h` - MQTT broker implementation
- `main/vehicle_detect.c/h` - Vehicle detection (automatic)
- `vehicle_fingerprints.json` - Fingerprint database (10 vehicles)

---

## Troubleshooting

### Broker Won't Start

**Check logs:**
```bash
idf.py monitor
```

Look for:
```
I (12345) app_main: Initializing MQTT broker on port 1883
I (12346) mqtt_broker: MQTT broker task created successfully
```

**Common issues:**
- Insufficient RAM - disable BLE to free memory
- Port already in use - change `mqtt_broker_port`
- WiFi not initialized - enable WiFi AP

### Can't Connect to Broker

1. ✅ Verify connected to WiCAN WiFi
2. ✅ Ping gateway: `ping 192.168.80.1`
3. ✅ Test with: `telnet 192.168.80.1 1883`
4. ✅ Check firewall settings

### Vehicle Detection Not Working

1. ✅ CAN bus must be active (engine running or key on)
2. ✅ Wait full 15 seconds for scan
3. ✅ Check fingerprints loaded: Look for log message
4. ✅ If no match, try manual profile selection

### No CAN Data

1. ✅ Enable MQTT client: `mqtt_en = "enable"`
2. ✅ Verify CAN bus connected
3. ✅ Check correct topics subscribed
4. ✅ Verify vehicle is sending frames

---

## Maintaining Your Fork

### Syncing with Upstream

```bash
# Setup (first time)
git remote add upstream https://github.com/meatpiHQ/wican-fw.git

# Update
git fetch upstream
git merge upstream/main

# Resolve conflicts in:
# - main/config_server.c
# - main/config_server.h
# - main/main.c
# - main/mqtt.c

# Test build
idf.py build

# Tag release
git tag v1.2.0-broker
git push origin v1.2.0-broker
```

GitHub Actions will auto-build binaries when you push a tag matching `v*-broker` or `v*-mqtt-broker`.

### File Change Summary

Keep track of modified files for easier merges:

✏️ **Modified:**
- `main/idf_component.yml`
- `main/CMakeLists.txt`
- `main/config_server.c/h`
- `main/main.c`
- `main/mqtt.c`

➕ **New:**
- `main/mqtt_broker.c/h`
- `main/vehicle_detect.c/h`
- `vehicle_fingerprints.json`
- `.github/workflows/build-release.yml`

---

## Comparison to Official Firmware

| Feature | Official WiCAN | + Broker & Detection |
|---------|----------------|----------------------|
| Firmware Size | ~1.2 MB | ~1.97 MB |
| RAM Usage | Base | +8 KB |
| MQTT Client | ✅ | ✅ |
| External Broker | ✅ | ✅ |
| **Local Broker** | ❌ | ✅ |
| **Vehicle Detection** | ❌ | ✅ |
| **Dynamic Subscriptions** | ❌ | ✅ |
| All Other Features | ✅ | ✅ Identical |

---

## Support & Resources

- 🐛 **Issues**: [GitHub Issues](https://github.com/adam-weber/wican-fw-mqtt-broker/issues)
- 🔧 **Upstream**: [Official WiCAN](https://github.com/meatpiHQ/wican-fw)
- 📚 **Mosquitto Docs**: [mosquitto.org](https://mosquitto.org)
- 🚗 **openpilot**: [github.com/commaai/openpilot](https://github.com/commaai/openpilot)

---

## License

Maintains **GPL-3.0** license from original WiCAN project by Meatpi Electronics.

---

## Credits

- **Original WiCAN**: [Meatpi Electronics](https://github.com/meatpiHQ)
- **Mosquitto**: [Eclipse Foundation](https://mosquitto.org)
- **Mosquitto ESP-IDF Port**: [Espressif Systems](https://developer.espressif.com/blog/2025/05/esp-idf-mosquitto-port/)
- **Vehicle Fingerprints**: Based on [comma.ai openpilot](https://github.com/commaai/openpilot)
