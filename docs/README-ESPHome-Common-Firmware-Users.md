# Common Firmware: Install and Update with ESPHome Web

This guide installs the released Common firmware with the browser. You do not need to install ESPHome or compile anything. The Common firmware runs on several boards and has no `secrets.yaml` file.

## Security Warning

This is the less secure option. The Common firmware does not use ESPHome API encryption, and its passwords are public because they are written in the files.

- **No API encryption:** anyone on your network can read meter readings from Home Assistant's ESPHome connection.
- **Setup password not set yet:** until you set a setup password, the `P1-Meter-Setup` access point is open to anyone nearby. Set the password right after the first start.
- **Public web server password:** the device's web page, including its firmware upload page, uses the public value `p1meter-setup`. Anyone who knows it can use those pages.
- **No OTA password:** network firmware updates are not protected by a password.

Use this only on a trusted home network, and never expose the device to the internet. If you want encrypted API access, use the [standard or advanced configuration](README-ESPHome-Python-CLI.md) instead.

## Supported Boards

| Board | Chip |
| --- | --- |
| Wemos D1 mini | ESP8266 |
| NodeMCU v2 | ESP8266 |
| ESP32 DevKit | ESP32 |
| ESP32-S2 Saola-1 | ESP32-S2 |
| ESP32-S3 DevKitC-1 | ESP32-S3 |
| ESP32-C3 DevKitM-1 | ESP32-C3 |

The ESP32-C6 is not supported.

## Install with ESPHome Web

Use Chrome or Edge, because the browser needs serial port access. Open [web.esphome.io](https://web.esphome.io/) over HTTPS.

1. Download the firmware for your board from the project's GitHub release. Use the `<device>-firmware.bin` file, for example `wemos-d1-mini-firmware.bin`.
2. Connect the board to your computer with a data-capable USB cable.
3. On the ESPHome Web page, click **Connect** and select the board's serial port.
4. Click **Install**, select the downloaded file, and write it from address 0. The file is a complete image.
5. Wait for the progress bar to finish. Unplug the board and power it on again.

If the port does not appear, try another cable or USB port, and close other programs that use the port.

## Set Up the Device

1. Connect to the `P1-Meter-Setup` Wi-Fi access point from your phone or computer.
2. Open `http://192.168.4.1/` in a browser.
3. Set a setup password (8 to 63 printable characters). The device then restarts.
4. Sign in with username `p1meter` and your new password, then configure Wi-Fi and MQTT as described in the [project README](../README.md#advanced-firmware-setup).

Until you set the setup password, the access point is open to anyone nearby.

## Update with the Web Page

Use this method for later updates instead of USB.

1. Download the `<device>-firmware.ota.bin` file from the same GitHub release. For boards that publish only `<device>-firmware.bin`, download that file instead.
2. Open the device's web page at its IP address, for example `http://192.168.1.50/`.
3. Sign in with username `p1meter` and the public web server password `p1meter-setup`, or your own password if you changed it.
4. Open the firmware upload page, select the downloaded file, and upload it.
5. Wait for the device to restart.

If the update fails, install the firmware again over USB as described above.

## Troubleshooting

- **Board not found on USB:** try another cable or USB port.
- **No meter values:** confirm the P1 interface reaches GPIO3 and the meter's P1 port is enabled. See the [project README](../README.md#p1-wiring).
