# P1 Meter Software Usage Guide

This guide explains how to complete the first setup, and use the web interface.

For hardware and wiring, see the [project README](../README.md).<br>
For building in VS Code, see the [VS Code guide](README-VSCode.md).<br>
For the ESPHome Dashboard, see the [Dashboard guide](README-ESPHome-Dashboard-Dockered.md).

## Contents

- [First Setup in the Web Interface](#first-setup-in-the-web-interface)
- [Web Interface Reference](#web-interface-reference)
- [Updates and Logs](#updates-and-logs)
- [Troubleshooting](#troubleshooting)
- [Security](#security)

## First Setup in the Web Interface

1. Connect the P1 interface and power the device.
2. Connect to the `P1-Meter-Setup` Wi-Fi network from your phone or computer.
   - Public: the network is open until you set a setup password in step 3.
   - Advanced: use the password from `p1_meter__setup_wifi_password`.
3. Open `http://192.168.4.1/` in a browser. Sign in with username `p1meter`.
   - Public: the **Password Setup** page opens Enter 8 to 63 printable characters. The device restarts, so reconnect to `P1-Meter-Setup` with the new password. Use username `p1meter` and the password you set.
   - Advanced: use username `p1meter` and the password from `p1_meter__setup_wifi_password`.
4. Open **Configure**, then:
   - Select a nearby Wi-Fi network, or enter its name, and enter its password.
   - Optionally enable MQTT and enter the broker host, port (default `1883`), username, and password.
   - Choose DHCP, or enter a static IP address, gateway, and subnet mask. DNS is optional.
   - Select **Save and reboot**.
6. The device joins your Wi-Fi and the `P1-Meter-Setup` network turns off. Use `http://p1-meter.local` or find the device's IP address in your router, then open it in a browser on your home network to access the web interface in the future.

Additional information:
- The setup network starts immediately if no Wi-Fi is saved. It also starts if the device cannot connect for 90 seconds. It turns off automatically once the device reconnects to your Wi-Fi.
- The sign-in prompt does not appear reliably on every device. If it does not appear, use a browser on a computer or open the page in a desktop browser.

## Web Interface Reference

The home page links to these pages. Use the sign-out icon in the top-right corner to end the session.

| Page | Path | Purpose |
| --- | --- | --- |
| Home | `/` | Opens the other pages. |
| Dashboard | `/dashboard` | Shows live electricity, gas, phase, meter, and power-quality values. |
| Configure | `/configure` | Sets Wi-Fi, optional MQTT, and the IP address mode. Saving reboots the device. |
| Change setup password | `/setup-password` | Sets the password for `P1-Meter-Setup` and the web interface. Saving reboots the device. |
| OTA Update | `/ota` | Uploads `firmware.ota.bin` for a wireless update. Keep power connected until the device restarts. |
| Factory Reset | `/factory-reset` | Erases saved Wi-Fi, MQTT, and network settings, then restarts in setup mode. |
| Reboot | `/reboot` | Restarts the device. |

## Updates
- **Update from Web interface:** to install a later version, download the newer file from the releases page. Open the `http://p1-meter.local` or the device's IP address in a browser on your home network, sign in, click **OTA Update**, and select the file. Keep the power connected until the device restarts.<br>
Upload the `<board>-firmware.ota.bin` file. For boards that publish only `<board>-firmware.bin`, upload that file.
- **Update from ESPHome Dashboard:** click install on your configuration, choose **On the network** from the pop-up window.
- **Update from VS Code:** once the device is on your network, use **P1 Meter: Upload firmware** from the VS Code tasks.

## Logs

- **In ESPHome Dashboard:** Use **Logs** action.
- **In VS Code:** use **P1 Meter: Show device logs** from the Tasks/PlatformIO.

## Troubleshooting

- **Status LED:** the device's LED has three states:<br>
Blinks every 250 ms while its setup network (`P1-Meter-Setup`) is active.<br>
Blinks once per second while disconnected.<br>
Stays on while Wi-Fi is connected.
- **Cannot open `http://192.168.4.1/`:** make sure your device is connected to `P1-Meter-Setup`, and try a browser on a computer.
- **Device does not join Wi-Fi:** check the SSID and password, and confirm the router offers 2.4 GHz Wi-Fi. The ESP8266 does not support 5 GHz.
- **Cannot browse to `http://p1-meter.local`:** Make sure that the device is connected to your local network and find the device's IP address in your router, then open it in a browser on your home network to access the web interface in the future.
- **No meter values:** confirm the P1 port is enabled and the wiring matches the [project README](../README.md#p1-wiring).
- **MQTT does not connect:** check the broker address, port, username, and password, then review the broker logs.

## Security

- **Public:** the API is not encrypted, and the web server password `p1meter-setup` and the OTA update are not protected by a private password. Anyone on your network can read meter values. Use it only on a trusted network, and never expose the device to the internet.
- **Advanced and Dashboard variants:** the API is encrypted with `p1_meter__encryption_key`, and OTA updates are protected by `p1meter__ota_password`. Keep these values private.
- **Web interface:** it uses unencrypted HTTP. Anyone connected to the setup network with its password can view saved Wi-Fi and MQTT passwords, change settings, update, reboot, or reset the device. Use it only on a trusted network.
