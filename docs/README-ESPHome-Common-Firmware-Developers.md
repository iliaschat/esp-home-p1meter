# Common Firmware: Build for Developers

This guide explains how to compile the Common firmware from `devices/` and how the released files are produced. Plain users should install the released files with ESPHome Web instead; see [Common Firmware: Install and Update with ESPHome Web](README-ESPHome-Common-Firmware-Users.md).

The Common firmware builds the same P1 meter firmware for several boards from one shared configuration, `p1meter_common.yaml`. No `secrets.yaml` file is needed. The GitHub release workflow builds these same files for every device on push to `public`.

## Security Warning

This is the less secure option. The Common firmware does not use ESPHome API encryption, and its passwords are public because they are written in the files.

- **No API encryption:** the ESPHome native API is not encrypted. Anyone on your network can read meter readings from Home Assistant's ESPHome connection.
- **Setup password not set yet:** until you set a setup password, the `P1-Meter-Setup` access point is open to anyone nearby.
- **Public web server password:** the web server, including its firmware upload page, uses the public value `p1meter-setup`. To change it, edit `web_server` `auth.password` in `p1meter_common.yaml` and recompile. The setup password is set from the web interface, not in the file.
- **No OTA password:** network firmware updates are not protected by a password.

Use this only on a trusted home network, and never expose the device to the internet.

## Released Files

Each device produces up to two files in the GitHub release:

- `<device>-firmware.bin`: the full image for USB installation at address 0. If the OTA image is identical, this is the only file published.
- `<device>-firmware.ota.bin`: the image for wireless updates through the web upload page. Published only when it differs from the full image.

The release workflow in `.github/workflows/release-public-firmware.yml` copies these from `devices/.esphome/build` after `esphome compile`. ESP8266 boards produce one identical image, so they publish only `<device>-firmware.bin`.

## Supported Devices

Each device file in `devices/` loads the shared settings, so every board gets the same sensors, web interface, and setup access point.

| Device file | Board | Chip | LED | Status |
| --- | --- | --- | --- | --- |
| `devices/wemos-d1-mini.yaml` | Wemos D1 mini (`d1_mini`) | ESP8266 | GPIO2, active low | Base device, tested with the project |
| `devices/nodemcuv2.yaml` | NodeMCU v2 (`nodemcuv2`) | ESP8266 | GPIO2, active low | Compiles; not tested on hardware |
| `devices/esp32-devkit.yaml` | ESP32 DevKit (`esp32dev`) | ESP32 | GPIO2, active high | Compiles; not tested on hardware |
| `devices/esp32-s2.yaml` | ESP32-S2 Saola-1 (`esp32-s2-saola-1`) | ESP32-S2 | RGB LED, GPIO18 | Compiles; not tested on hardware |
| `devices/esp32-s3.yaml` | ESP32-S3 DevKitC-1 (`esp32-s3-devkitc-1`) | ESP32-S3 | RGB LED, GPIO48 | Compiles; not tested on hardware |
| `devices/esp32-c3.yaml` | ESP32-C3 DevKitM-1 (`esp32-c3-devkitm-1`) | ESP32-C3 | RGB LED, GPIO8 | Compiles; not tested on hardware |

"Compiles" means the configuration validates with `esphome config`. Build and flashing for these boards have not been tested on hardware.

**Not supported:** the ESP32-C6. ESPHome supports only ESP-IDF for this chip, while the setup page code requires the Arduino framework on ESP32.

**Board notes:**

- All boards read the meter on GPIO3 (`RX`), the same as the Wemos D1 mini. Check that GPIO3 is usable on your board before connecting the P1 interface.
- The ESP32-S2, S3, and C3 use an addressable RGB LED, which the firmware drives as a single white light. Confirm the LED pin against your board's documentation.
- The ESP32 firmware uses Arduino framework 3.3.7. The first compile downloads its toolchain, which takes several minutes.

## Before You Start

1. Install ESPHome as described in the [command-line guide](README-ESPHome-Python-CLI.md#install-esphome).
2. Open a terminal in the project folder, the folder that contains `p1meter_common.yaml` and `devices/`.
3. Activate the ESPHome environment as described in the install steps.

Run all commands from the project folder so ESPHome can find the `components/` folder. This guide covers the command line only; the ESPHome Dashboard is not covered for these files.

## Validate, Compile, and Flash

Choose your device file from the table above. Validate the configuration first. This checks the YAML and board name without downloading the toolchain:

```sh
esphome config devices/esp32-devkit.yaml
```

Compile the firmware without flashing:

```sh
esphome compile devices/esp32-devkit.yaml
```

Connect the board with a data-capable USB cable, then compile and flash in one step:

```sh
esphome run devices/esp32-devkit.yaml
```

Select the serial port if prompted. Replace `esp32-devkit.yaml` with your device file. Common port names are `COM3` on Windows, `/dev/ttyUSB0` or `/dev/ttyACM0` on Linux, and `/dev/cu.usbserial-*` on macOS.

To upload an already compiled firmware to a specific port:

```sh
esphome upload devices/esp32-devkit.yaml --device /dev/ttyUSB0
```

On Windows PowerShell, use `--device COM3` or the port your board uses.

View the device logs without flashing:

```sh
esphome logs devices/esp32-devkit.yaml
```

After the device is on your Wi-Fi, run `esphome run` again and select the network device, or pass its IP address with `--device`. Use USB if the network update fails.

## Setup After Flashing

1. Connect to the `P1-Meter-Setup` Wi-Fi access point from your phone or computer.
2. Open `http://192.168.4.1/` in a browser.
3. Set a setup password (8 to 63 printable characters). The device then restarts.
4. Sign in with username `p1meter` and your new password, then configure Wi-Fi and MQTT as described in the [project README](../README.md#advanced-firmware-setup).

Until you set the setup password, the access point is open to anyone nearby.

## Troubleshooting

- **`esphome config` fails with a board error:** check that the device file's board name matches the table above.
- **Board not found on USB:** try another cable or USB port.
- **Build fails on an unlisted chip:** the ESP32-C6 and other unlisted chips are not supported.
- **No meter values:** confirm the P1 interface reaches GPIO3 and the meter's P1 port is enabled. See the [project README](../README.md#p1-wiring).
