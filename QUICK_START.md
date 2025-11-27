# WiCAN with MQTT Broker - Quick Start

## For Users: How to Flash and Use

### Step 1: Download Firmware

Go to [Releases](https://github.com/YOUR-USERNAME/wican-fw-broker/releases) and download:
- `wican-broker-vX.X.X-merged.bin` (recommended - includes everything)

### Step 2: Flash to WiCAN Device

**Windows:**
1. Download [ESP Flash Tool](https://www.espressif.com/en/support/download/other-tools)
2. Open tool, select ESP32-C3
3. Load the `.bin` file at offset `0x0`
4. Select COM port
5. Click "START"

**Mac/Linux:**
```bash
# Install esptool
pip install esptool

# Flash (replace /dev/ttyUSB0 with your port)
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x0 wican-broker-vX.X.X-merged.bin
```

**Find your port:**
- Mac: `/dev/tty.usbserial-*` or `/dev/tty.SLAB_USBtoUART`
- Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
- Windows: `COM3`, `COM4`, etc. (check Device Manager)

### Step 3: Enable MQTT Broker

1. **Connect to WiFi**:
   - Network: `WiCAN_XXXXXX` (where XXXXXX is from device MAC)
   - Password: `@meatpi#`

2. **Open Web Interface**:
   - Browser: `http://192.168.80.1` (or `192.168.0.10` for WiCAN Pro)

3. **Enable Features**:
   - Scroll to MQTT section
   - Set `MQTT Broker Enable` to `enable`
   - Set `MQTT Enable` to `enable` (this is the client)
   - Optionally change `MQTT Broker Port` (default: `1883`)
   - Click **Save**
   - Device will reboot

### Step 4: Connect Your Apps

Now any device on the WiCAN WiFi can connect to the MQTT broker:

**Connection Details:**
- **Host**: `192.168.80.1` (or `192.168.0.10` for Pro)
- **Port**: `1883`
- **Topics**:
  - `wican/YOUR_DEVICE_ID/can/tx` - CAN messages from vehicle
  - `wican/YOUR_DEVICE_ID/can/status` - Device status

**Example with MQTT Explorer (GUI app):**
1. Download [MQTT Explorer](http://mqtt-explorer.com/)
2. Create connection:
   - Host: `192.168.80.1`
   - Port: `1883`
3. Connect and browse topics

**Example with Python:**
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

**Example with mosquitto_sub (command line):**
```bash
mosquitto_sub -h 192.168.80.1 -p 1883 -t "wican/+/can/tx" -v
```

---

## For Developers: How to Build from Source

### Prerequisites

- ESP-IDF v5.0+ ([installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/))
- Git
- Python 3.7+

### Build Steps

```bash
# Clone this repo
git clone https://github.com/YOUR-USERNAME/wican-fw-broker.git
cd wican-fw-broker

# Setup ESP-IDF environment
. ~/esp/esp-idf/export.sh  # or wherever you installed ESP-IDF

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

### Build Script

For release builds:
```bash
./build-all-variants.sh
```

Outputs to `releases/` folder.

---

## Maintaining Your Fork

### When WiCAN Releases Updates

```bash
# Add upstream remote (first time only)
git remote add upstream https://github.com/meatpiHQ/wican-fw.git

# Fetch latest from official repo
git fetch upstream

# Merge into your branch
git merge upstream/main

# Resolve any conflicts in:
# - main/config_server.c
# - main/config_server.h
# - main/main.c
# - main/mqtt.c

# Test build
idf.py build

# Tag new version
git tag v1.1.0-broker
git push origin v1.1.0-broker

# GitHub Actions will auto-build and create release
```

### Files You Modified

Keep track of these for merges:
- ✏️ `main/config_server.c` - Added broker config parsing
- ✏️ `main/config_server.h` - Added broker config fields
- ✏️ `main/idf_component.yml` - Added mosquitto component
- ✏️ `main/main.c` - Added broker initialization
- ✏️ `main/mqtt.c` - Added localhost detection
- ➕ `main/mqtt_broker.c` - New file (broker impl)
- ➕ `main/mqtt_broker.h` - New file (broker API)

---

## Troubleshooting

### Can't Flash Device

**Error: "Failed to connect"**
- Hold BOOT button while connecting USB
- Try different USB cable
- Check driver installed (CP210x or CH340)

**Error: "Permission denied"**
```bash
# Linux: Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Broker Won't Start

Check logs:
```bash
idf.py monitor
```

Look for:
```
I (12345) app_main: Initializing MQTT broker on port 1883
I (12346) mqtt_broker: MQTT broker task created successfully
```

**If not appearing:**
- Verify `mqtt_broker_en` is set to `"enable"`
- Check available RAM (broker needs ~6KB)
- Try disabling BLE to free memory

### Can't Connect to Broker

1. ✅ Verify connected to WiCAN WiFi
2. ✅ Ping gateway: `ping 192.168.80.1`
3. ✅ Test with telnet: `telnet 192.168.80.1 1883`
4. ✅ Check firewall on your device

### No CAN Data

1. ✅ Enable MQTT client: `mqtt_en = "enable"`
2. ✅ Verify CAN bus connected and active
3. ✅ Check vehicle is sending CAN frames
4. ✅ Subscribe to correct topic pattern: `wican/+/can/tx`

---

## What's Different from Official WiCAN?

| Feature | Official | + Broker |
|---------|----------|----------|
| Size | ~1.2 MB | ~1.26 MB |
| RAM Usage | Base | +6KB |
| MQTT Client | ✅ | ✅ |
| Local Broker | ❌ | ✅ |
| Internet Required | For MQTT | No |
| Multi-subscriber | Via cloud | Local WiFi |

Everything else is identical!

---

## Support

- 📖 Full docs: [MQTT_BROKER_README.md](./MQTT_BROKER_README.md)
- 🐛 Issues: [GitHub Issues](https://github.com/YOUR-USERNAME/wican-fw-broker/issues)
- 💬 Discussions: [GitHub Discussions](https://github.com/YOUR-USERNAME/wican-fw-broker/discussions)
- 🔧 Official WiCAN: [meatpiHQ/wican-fw](https://github.com/meatpiHQ/wican-fw)
