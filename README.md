# UGreen USB Hub S2 Mini Hack

A DIY smart USB switch built with a Lolin S2 Mini (ESP32-S2) that allows switching a UGreen USB hub between two computers (PC/Mac) via MQTT and Home Assistant.

## Features

- **WiFiManager** - Web-based WiFi configuration with captive portal
- **MQTT Integration** - Control via Home Assistant or any MQTT client
- **Home Assistant Auto-Discovery** - Automatically appears in HA as a switch entity
- **OTA Updates** - Update firmware over WiFi via ElegantOTA
- **Web UI** - Control and monitor the switch via browser
- **Non-blocking MQTT** - Web interface always accessible even if MQTT is down
- **LittleFS Config Storage** - Settings persist across reboots

## Hardware

- **Lolin S2 Mini** (ESP32-S2)
- **UGreen USB Hub** (modified with relay control)
- GPIO35 - Switch control pin
- GPIO39 - Monitor pin (reads current state)
- GPIO15 - Onboard LED

## First-Time Setup

1. **Flash the firmware** via USB using PlatformIO
2. **Connect to WiFi AP** - Look for `UGreen-USB-Hub-Setup` network
3. **Open captive portal** - Should open automatically, or go to `http://192.168.4.1`
4. **Configure settings**:
   - WiFi SSID and password
   - MQTT Broker IP (e.g., `192.168.0.112`)
   - MQTT Port (default: `1883`)
   - MQTT Username (optional)
   - MQTT Password (optional)
5. **Save** - Device will restart and connect to your network

## Web Interface

Once connected, access the device at its IP address or hostname:

| Endpoint | Description |
|----------|-------------|
| `http://<device-ip>/` | Main web UI - status, switch control, HA discovery |
| `http://<device-ip>/update` | OTA firmware update page |
| `http://<device-ip>/reset` | Reset WiFi/MQTT config and restart in AP mode |
| `http://<device-ip>/state` | Get current state (PC/Mac) as plain text |
| `http://<device-ip>/switch` | Toggle the switch |
| `http://<device-ip>/discovery_on` | Enable Home Assistant MQTT discovery |
| `http://<device-ip>/discovery_off` | Remove device from Home Assistant |

## MQTT Topics

| Topic | Description |
|-------|-------------|
| `home/usb_hub_switch/state` | Current state (`PC` or `Mac`) - retained |
| `home/usb_hub_switch/switch/set` | Command topic - send `PC` or `Mac` |
| `stat/usb_hub_switch/availability` | Online status (`online`/`offline`) - retained |

## Home Assistant Integration

The device supports MQTT auto-discovery. To add it to Home Assistant:

1. Ensure MQTT integration is configured in HA
2. Access the device web UI
3. Click **"Add to HA"** button
4. The switch will appear in Home Assistant as "Ugreen USB Hub Switch"

## OTA Updates

1. Build new firmware: `pio run`
2. Access `http://<device-ip>/update`
3. Upload the `.bin` file from `.pio/build/lolin_s2_mini/firmware.bin`

Or configure PlatformIO for OTA uploads by uncommenting these lines in `platformio.ini`:
```ini
extra_scripts = platformio_upload.py
upload_protocol = custom
custom_upload_url = http://<device-ip>/update
```

## Resetting Configuration

If you need to reconfigure WiFi or MQTT settings:

**Option 1:** Access `http://<device-ip>/reset` from the web UI

**Option 2:** Erase flash and re-upload:
```bash
pio run --target erase --target upload
```

## Building & Flashing

### Prerequisites
- [PlatformIO](https://platformio.org/) (VSCode extension or CLI)
- USB cable for initial flash

### Initial Flash (USB)
1. Hold **BOOT** button on S2 Mini
2. Connect USB while holding BOOT
3. Release after 1 second
4. Run: `pio run --target upload`

### Subsequent Updates (OTA)
Use the web UI at `/update` or configure PlatformIO for OTA uploads.

## Troubleshooting

### Device not creating WiFi AP
- Old credentials may be stored in flash
- Erase flash completely: `pio run --target erase --target upload`

### Can't connect to MQTT
- Check broker IP and port in web UI
- Verify MQTT credentials
- MQTT status shown on main web page

### Web UI not responding
- The non-blocking MQTT ensures web UI is always accessible
- If unresponsive, device may need power cycle

## License

MIT
