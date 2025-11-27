# WiCAN MQTT Broker Feature

## Overview

This firmware now includes an **embedded MQTT broker** that runs directly on your WiCAN ESP32 device. This turns your WiCAN into a central gateway for vehicle diagnostics, allowing multiple devices in your vehicle to subscribe to CAN bus data without requiring an external cloud MQTT broker.

## What This Means For You

### Before (Standard Setup)
```
WiCAN Device → Internet → Cloud MQTT Broker → Your Apps
```

### After (With Local Broker)
```
WiCAN Device (with built-in broker) ← WiFi → Your Apps/Devices
```

## Key Benefits

1. **No Internet Required**: The broker runs locally on the WiCAN device - perfect for vehicles without constant internet connectivity
2. **Lower Latency**: Direct WiFi connection to the broker means faster data delivery
3. **Privacy**: All vehicle data stays local unless you choose to forward it
4. **Multiple Subscribers**: Multiple devices (phone, tablet, laptop) can subscribe to CAN data simultaneously
5. **Standard MQTT**: Uses industry-standard Mosquitto broker - compatible with any MQTT client

## How It Works

1. **WiFi Access Point**: WiCAN creates a WiFi AP (e.g., `WiCAN_84f703406f75`)
2. **MQTT Broker**: Runs on the WiCAN device on port 1883 (configurable)
3. **MQTT Client**: Auto-publishes CAN bus data to the local broker
4. **Your Devices**: Connect to WiCAN WiFi and subscribe to MQTT topics

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    WiCAN ESP32 Device                   │
│  ┌──────────────┐                                       │
│  │   CAN Bus    │                                       │
│  │  Interface   │                                       │
│  └──────┬───────┘                                       │
│         │                                               │
│         ▼                                               │
│  ┌──────────────┐      ┌──────────────────┐            │
│  │ MQTT Client  │─────►│  MQTT Broker     │            │
│  │  Publisher   │      │  (Mosquitto)     │            │
│  └──────────────┘      │  Port: 1883      │            │
│                        └─────────┬────────┘            │
│                                  │                      │
│                        ┌─────────▼────────┐            │
│                        │   WiFi AP        │            │
│                        │   192.168.80.1   │            │
│                        └─────────┬────────┘            │
└──────────────────────────────────┼──────────────────────┘
                                   │
           ┌───────────────────────┼───────────────────────┐
           │                       │                       │
      ┌────▼─────┐           ┌────▼─────┐          ┌─────▼──────┐
      │  Phone   │           │  Tablet  │          │   Laptop   │
      │  (MQTT   │           │  (MQTT   │          │   (MQTT    │
      │  Client) │           │  Client) │          │   Client)  │
      └──────────┘           └──────────┘          └────────────┘
```

## Quick Start

### 1. Enable the MQTT Broker

**Via Web Interface:**
1. Connect to your WiCAN WiFi AP (e.g., `WiCAN_84f703406f75`)
2. Open browser to `http://192.168.80.1` (or `192.168.0.10` for WiCAN Pro)
3. Navigate to MQTT settings
4. Set `mqtt_broker_en` to `enable`
5. Optionally change `mqtt_broker_port` (default: `1883`)
6. Enable MQTT client: `mqtt_en` to `enable`
7. Save and reboot

**Via REST API:**
```bash
curl -X POST http://192.168.80.1/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "mqtt_broker_en": "enable",
    "mqtt_broker_port": "1883",
    "mqtt_en": "enable",
    "mqtt_tx_topic": "wican/YOUR_DEVICE_ID/can/tx",
    "mqtt_rx_topic": "wican/YOUR_DEVICE_ID/can/rx",
    "mqtt_status_topic": "wican/YOUR_DEVICE_ID/can/status"
  }'
```

### 2. Connect Your Devices

**From a Phone/Tablet/Laptop:**

1. Connect to WiCAN WiFi network
2. Use any MQTT client app (e.g., MQTT Dash, IoT MQTT Panel, mosquitto_sub)
3. Connect to broker:
   - **Host**: `192.168.80.1` (or `192.168.0.10` for Pro)
   - **Port**: `1883`
   - **Username**: (leave blank or configure in WiCAN settings)
   - **Password**: (leave blank or configure in WiCAN settings)

**Example using mosquitto_sub (command line):**
```bash
# Subscribe to all CAN messages
mosquitto_sub -h 192.168.80.1 -p 1883 -t "wican/+/can/tx" -v

# Subscribe to status messages
mosquitto_sub -h 192.168.80.1 -p 1883 -t "wican/+/can/status" -v
```

**Example using Python (paho-mqtt):**
```python
import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    client.subscribe("wican/+/can/tx")

def on_message(client, userdata, msg):
    print(f"{msg.topic}: {msg.payload.decode()}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("192.168.80.1", 1883, 60)
client.loop_forever()
```

## Default Topics

When the broker is enabled, the WiCAN client publishes to these topics:

- `wican/{DEVICE_ID}/can/tx` - CAN frames from the vehicle bus
- `wican/{DEVICE_ID}/can/rx` - CAN frames sent to the vehicle bus
- `wican/{DEVICE_ID}/can/status` - Device status (online/offline)

## Configuration Options

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `mqtt_broker_en` | string | `"disable"` | Enable/disable the local broker (`"enable"` or `"disable"`) |
| `mqtt_broker_port` | string | `"1883"` | Port number for the broker (1-65535) |

## Memory Requirements

The Mosquitto broker requires approximately:
- **Flash**: ~60 KB
- **RAM**: ~2 KB at startup + ~4 KB per connected client
- **Stack**: 6 KB per broker task

## Limitations

1. **Single Transport**: Only plain TCP MQTT is supported. TLS/SSL can be added later if needed.
2. **QoS Support**: QoS 0 and 1 are supported. QoS 2 support depends on Mosquitto configuration.
3. **Authentication**: Currently no authentication by default (can be added in future updates)
4. **Persistence**: No message persistence across reboots
5. **Max Clients**: Limited by available RAM (~4KB per client)

## Building the Firmware

### Prerequisites

- ESP-IDF v5.0 or later
- WiCAN hardware (ESP32-C3 based)

### Build Steps

```bash
# The Mosquitto component will be auto-downloaded during build
idf.py build

# Flash to device
idf.py flash

# Monitor logs
idf.py monitor
```

The build system will automatically:
1. Download the `espressif/mosquitto` component
2. Configure it for ESP32-C3
3. Compile and link it with the firmware

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
I (12347) mqtt_broker: Starting MQTT broker on port 1883
```

**Common issues:**
- Insufficient RAM - reduce max clients or disable other features
- Port already in use - change `mqtt_broker_port`
- WiFi not initialized - ensure WiFi AP is enabled

### Can't Connect from Client

1. Verify you're connected to WiCAN WiFi
2. Ping the gateway: `ping 192.168.80.1`
3. Check broker is running: `telnet 192.168.80.1 1883`
4. Verify firewall settings on your device

### No CAN Data Appearing

1. Ensure MQTT client is enabled: `mqtt_en = "enable"`
2. Check CAN bus is active and receiving frames
3. Verify topics match in your subscription
4. Check device_id in the web interface

## Advanced Usage

### Bridging to External Broker

To forward data to a cloud broker while keeping the local broker:

1. Keep `mqtt_broker_en` = `"enable"`
2. This setup keeps data local AND publishes to localhost
3. For cloud forwarding, you would need dual-client mode (future enhancement)

### Custom Topics

Edit the topic names in the WiCAN web interface:
- `mqtt_tx_topic`: Where CAN TX frames are published
- `mqtt_rx_topic`: Where CAN RX frames are published
- `mqtt_status_topic`: Where status messages are published

## Resources

- **Espressif Mosquitto Port**: [Developer Portal Article](https://developer.espressif.com/blog/2025/05/esp-idf-mosquitto-port/)
- **Mosquitto Documentation**: [eclipse.org/mosquitto](https://mosquitto.org)
- **WiCAN Project**: [github.com/meatpiHQ/wican-fw](https://github.com/meatpiHQ/wican-fw)

## Support

For issues or questions:
1. Check the logs: `idf.py monitor`
2. Review this documentation
3. Open an issue on GitHub with logs and configuration

## License

This feature maintains the WiCAN project's GPL-3.0 license.

---

**Note**: This is a powerful feature that makes your WiCAN device a true standalone vehicle gateway. No cloud required!
