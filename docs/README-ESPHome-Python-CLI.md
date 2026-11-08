# Compile and Install with ESPHome

This guide uses the ESPHome command-line tool. For the VS Code dev container, see the [VS Code guide](README-VSCode.md). For hardware and wiring, see the [project README](../README.md).

## Contents

- [Install ESPHome](#install-esphome)
   - [Linux and macOS](#linux-and-macos)
   - [Windows PowerShell](#windows-powershell)
- [Credentials and Configuration](#credentials-and-configuration)
- [Compile and Flash](#compile-and-flash)

## Install ESPHome

Get the project files, then open a terminal in the project folder. Use PowerShell on Windows or Terminal on Linux or macOS. See [Useful Terms](../README.md#useful-terms) for common ESPHome terms.

- **Download:** Open the [project on GitHub](https://github.com/iliaschat/p1meter-dsmr-esphome), click **Code**, then **Download ZIP**. Extract the ZIP file and open the resulting `p1meter-dsmr-esphome` folder.
- **Git:** If [Git](https://git-scm.com/downloads) is installed, run this in a terminal and then open the new folder:

	```sh
	git clone https://github.com/iliaschat/p1meter-dsmr-esphome.git
	```

Install [Python 3](https://www.python.org/downloads/) and ESPHome in a virtual environment, a separate folder for this project's Python tools. Internet access is required for installation and the first compilation, which downloads the ESP8266 toolchain.

### Linux and macOS

Run:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install esphome
```

In each new terminal, return to the project folder and run `source .venv/bin/activate` before running ESPHome commands.

### Windows PowerShell

Run:

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install esphome
$env:Path = "$PWD\.venv\Scripts;$env:Path"
```

In each new PowerShell window, return to the project folder and run `$env:Path = "$PWD\.venv\Scripts;$env:Path"` before running ESPHome commands. This changes the search path only for that window and avoids changing PowerShell's script execution policy. If `py` is not found, install Python and enable the Python launcher during installation.

The `.venv/` directory is ignored by Git. The ESPHome commands below work in either terminal after completing the steps for your operating system.

## Credentials and Configuration

1. Create `secrets.yaml` in the project directory, or edit the file created by the dev container. If the container just created it from `secrets-example.yaml`, replace the sample contents with the following. Otherwise, add the missing entries without deleting existing secrets or creating duplicate names. Enter your own values:

   ```yaml
   p1_meter__setup_wifi_password: "YOUR_SETUP_WIFI_PASSWORD"
   p1_meter__encryption_key: "YOUR_P1_METER_ENCRYPTION_KEY"
   p1meter__ota_password: "YOUR_OTA_PASSWORD"
   wifi_ssid: "YOUR_WIFI_SSID"
   wifi_password: "YOUR_WIFI_PASSWORD"
   mqtt_broker: "YOUR_MQTT_BROKER"
   mqtt_username: "YOUR_MQTT_USERNAME"
   mqtt_password: "YOUR_MQTT_PASSWORD"
   ```

   Keep this file private. Both configurations use `p1_meter__encryption_key` to encrypt the ESPHome API and `p1meter__ota_password` to protect ESPHome OTA updates. The advanced configuration also uses `p1_meter__setup_wifi_password` for the setup Wi-Fi and web sign-in. It must be at least eight characters. Its sign-in username is `p1meter`.

2. Choose a configuration file:

   - `p1meter_wemosd1mini_standard.yaml` uses the Wi-Fi and MQTT settings in `secrets.yaml`. MQTT is disabled by default; configure its broker credentials before enabling the Home Assistant **MQTT** switch. The switch state is saved in flash and restored on boot.
   - `p1meter_wemosd1mini_advanced.yaml` provides a setup access point and web interface at `http://192.168.4.1/`. Its MQTT setting is shared with the Home Assistant **MQTT** switch.

   Confirm that your meter sends DSMR data at the configured serial settings. Both configurations set `crc_check: false`, disabling telegram CRC checking.

3. Validate the chosen configuration from the project directory. For example:

   ```sh
   esphome config p1meter_wemosd1mini_standard.yaml
   ```

   Substitute `p1meter_wemosd1mini_advanced.yaml` for the advanced configuration in this and the following commands. The advanced YAML loads `p1_setup` from the local `components/` directory. If compiling a copy without that directory, replace its `external_components` block with:

   ```yaml
   external_components:
     - source: github://iliaschat/p1meter-dsmr-esphome
       components: [p1_setup]
   ```

   The GitHub component is downloaded automatically.

## Compile and Flash

For a first installation, connect the D1 mini to the computer running ESPHome with a data-capable USB cable. Then compile and flash the chosen configuration:

```sh
esphome run p1meter_wemosd1mini_standard.yaml
```

Select the serial port if prompted. Common port names include `COM3` on Windows, `/dev/ttyUSB0` or `/dev/ttyACM0` on Linux, and `/dev/cu.usbserial-*` on macOS; the actual name can differ. After flashing, connect the P1 interface and power the device. With the advanced firmware, complete the [Advanced Firmware Setup](../README.md#advanced-firmware-setup) steps. On Windows with WSL2, follow the [USB attachment instructions](README-VSCode.md#usb-flashing-with-wsl2) before flashing.

The advanced web interface uses the browser's native Digest sign-in prompt, with username `p1meter` and `p1_meter__setup_wifi_password`. The top-right sign-out icon revokes the session and attempts to close the page; browser restrictions may leave a signed-out confirmation visible instead.

To compile and upload separately on Linux or macOS, use:

```sh
esphome compile p1meter_wemosd1mini_standard.yaml
esphome upload p1meter_wemosd1mini_standard.yaml --device /dev/ttyUSB0
```

On Windows PowerShell, use the device's COM port instead:

```powershell
esphome compile p1meter_wemosd1mini_standard.yaml
esphome upload p1meter_wemosd1mini_standard.yaml --device COM3
```

Replace the example device path or COM port with the actual serial port. Once the device is on Wi-Fi, run the same `esphome run` command and select the network device, or specify `--device` followed by its IP address. For the standard configuration, `wifi.use_address` can be set to the device's current IP address or resolvable hostname when connecting to an installed device; it controls where ESPHome connects and does not assign a static IP. Use a router DHCP reservation for a stable address, or omit `use_address` for discovery or initial USB flashing.

To view logs without flashing, run the command for your configuration:

```sh
esphome logs p1meter_wemosd1mini_standard.yaml
```

or
```sh
esphome logs p1meter_wemosd1mini_advanced.yaml
```
