# Compile and Install with the ESPHome Dashboard

This guide uses the ESPHome Device Builder dashboard running in Docker. For hardware and wiring, see the [project README](../README.md). For the command-line and VS Code alternatives, see the [ESPHome CLI guide](README-ESPHome-Python-CLI.md) and [VS Code guide](README-VSCode.md).

## Contents

- [Before You Start](#before-you-start)
- [Start the Dashboard](#start-the-dashboard)
  - [Linux](#linux)
  - [macOS](#macos)
  - [Windows PowerShell](#windows-powershell)
- [Choose a Configuration](#choose-a-configuration)
- [Add Your Secrets](#add-your-secrets)
- [Set Up the Device in the Dashboard](#set-up-the-device-in-the-dashboard)
  - [Advanced Configuration: Fetch the Component](#advanced-configuration-fetch-the-component)
- [Validate and Compile](#validate-and-compile)
- [First Installation over USB](#first-installation-over-usb)
- [Later Updates and Logs](#later-updates-and-logs)

## Before You Start

You need a Wemos D1 mini, a USB data cable, [Docker](https://docs.docker.com/get-docker/), and an internet connection. For explanations of terms like firmware, YAML, and MQTT, see [Useful Terms](../README.md#useful-terms). If you already have the ESPHome dashboard running in Docker, open it and continue at [Choose a Configuration](#choose-a-configuration).

You do not need to download the project code to use the Dashboard. Create a folder for your YAML configuration and `secrets.yaml`; the advanced configuration can fetch its component from GitHub. Docker Desktop users should ensure the folder is accessible to Docker.

## Start the Dashboard

1. Install Docker using the [Docker installation guide](https://docs.docker.com/get-docker/). On Windows or macOS, install Docker Desktop. Start Docker or Docker Desktop and wait for it to be ready.
2. Open a terminal on the computer running Docker: search for **PowerShell** in the Windows Start menu, or open **Terminal** on Linux or macOS.
3. Create or choose a folder for your ESPHome configuration files. In the terminal, use `cd ` and drag that folder into the window, then press Enter. Alternatively, type `cd "path to the folder"`.
4. Copy and run the command below for your computer. The first run downloads the dashboard and may take a few minutes.

### Linux

```sh
docker run --name esphome-dashboard --rm --network host -v "$PWD:/config" ghcr.io/esphome/esphome:2026.10.2
```

### macOS

```sh
docker run --name esphome-dashboard --rm -p 127.0.0.1:6052:6052 -v "$PWD:/config" ghcr.io/esphome/esphome:2026.10.2
```

### Windows PowerShell

```powershell
docker run --name esphome-dashboard --rm -p 127.0.0.1:6052:6052 --mount "type=bind,source=$($PWD.Path),target=/config" ghcr.io/esphome/esphome:2026.10.2
```

All commands mount the current folder as the Dashboard's configuration folder. It only needs your YAML files and `secrets.yaml`; you do not need to download the project code. Leave the terminal open and visit `http://localhost:6052` in a browser on the same computer. To stop the Dashboard, press Ctrl+C. If port 6052 is already in use on macOS or Windows, change the host port in the command from `127.0.0.1:6052:6052` to `127.0.0.1:6053:6052` and visit `http://localhost:6053`.

The Linux command uses host networking so the Dashboard can discover devices and their online status using mDNS, whether they use DHCP or a static IP. Host networking may expose the Dashboard to other computers; use a firewall or dashboard authentication to restrict access. The macOS and Windows commands bind the Dashboard to the host's loopback interface, but `.local` discovery may not work with port publishing. Docker Desktop 4.34 and later also supports host networking when enabled in its settings; see the [ESPHome Docker documentation](https://esphome.io/install/docker/) for details. The Dashboard can read and modify the mounted project files, including secrets; do not expose it to untrusted networks.

If the folder already contains configuration files, they may appear as device cards. Choose only one configuration for your meter.

## Choose a Configuration

Choose **one** configuration for your meter. Open its YAML file on GitHub and copy the file contents into the Dashboard. You do not need to download the repository or component code.

- [Standard configuration](../p1meter_wemosd1mini_standard.yaml): enter your Wi-Fi details in the dashboard before installing. MQTT is optional.
- [Advanced configuration](../p1meter_wemosd1mini_advanced.yaml): connect to the meter's `P1-Meter-Setup` Wi-Fi after installation, then open `http://192.168.4.1/` in a browser to enter Wi-Fi and optional MQTT settings.
- [Common firmware for developers](README-ESPHome-Common-Firmware-Developers.md): a variant with no secrets file and public passwords for several boards. It is less secure, and it is not set up through this dashboard; read the guide first.

Both configurations name the device `p1-meter`. They are alternatives for one device, not two separate devices. If you have more than one meter, give each device a different `name` in its configuration.

## Add Your Secrets

Secrets are the passwords and other private settings used by the configuration. In the dashboard, open **Secrets** (look for the key icon or the dashboard menu). This opens the shared `secrets.yaml` file. If you see individual fields instead of text, switch to the **YAML** view.

Add the following entries to `secrets.yaml`. Keep the quotation marks. If the file already has secrets, add only the missing entries.

```yaml
p1_meter__setup_wifi_password: "YOUR_SETUP_WIFI_PASSWORD"
p1_meter__encryption_key: "YOUR_P1_METER_ENCRYPTION_KEY"
p1meter__ota_password: "YOUR_OTA_PASSWORD"
wifi_ssid: "YOUR_WIFI_SSID"
wifi_password: "YOUR_WIFI_PASSWORD"
mqtt_broker: ""
mqtt_username: ""
mqtt_password: ""
```

Both configurations require `p1_meter__encryption_key` for the encrypted ESPHome native API and `p1meter__ota_password` for ESPHome OTA updates. For the **standard configuration**, replace `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` with your 2.4 GHz Wi-Fi name and password. You may leave `YOUR_SETUP_WIFI_PASSWORD` unchanged because this configuration does not use it.

For the **advanced configuration**, set `YOUR_SETUP_WIFI_PASSWORD` to a password of at least eight characters. It protects the setup access point and web interface. The sign-in username is `p1meter`. You will enter your home Wi-Fi and optional MQTT settings on the device after installation.

If you do not use MQTT, leave the three MQTT entries as shown and keep the device's MQTT switch disabled.

Keep these values private. If a value contains a double quote or backslash, escape it with a backslash in the editor. Do not share screenshots containing your secrets.

## Set Up the Device in the Dashboard

If the Dashboard already has a card for your configuration, click **Edit** and switch to the **YAML** view if needed. Otherwise, create a device:

1. Click **Create device** (or **New device**) and choose **Empty Configuration**. Give it a name and finish creating it.
2. On the new device's card, click **Edit**. If the editor shows individual settings instead of the configuration text, switch to the **YAML** view.
3. Open the YAML file for your chosen configuration from [Choose a Configuration](#choose-a-configuration). On GitHub, click **Raw**, then copy and paste the entire file into the Dashboard editor.
4. Select all the text in the editor and replace it with the copied configuration. Preserve the spaces at the beginning of each line; do not replace them with tabs.
5. If you chose the advanced configuration, follow the next section before saving. Otherwise, click **Save** and continue to [Validate and Compile](#validate-and-compile).

### Advanced Configuration: Fetch the Component

The advanced configuration uses `p1_setup`. You do not need to download it. In the copied YAML, replace the `external_components` block near the top:

```yaml
external_components:
  - source:
      type: local
      path: components
```

Replace just that block with the following, keeping the spaces as shown:

```yaml
external_components:
  - source: github://iliaschat/p1meter-dsmr-esphome
    components: [p1_setup]
```

ESPHome fetches only this component from GitHub during validation; it does not download the rest of the project. The Dashboard needs internet access. Click **Save** when finished. The standard configuration needs no external component.

## Validate and Compile

1. Return to the dashboard and find the card for the configuration you chose or created. Do not install both configuration cards on the same meter.
2. Use the card's menu and select **Validate**. Resolve any errors before proceeding, especially missing secrets or the advanced configuration's component source.
3. Select **Install**, then **Manual download**. The dashboard builds the firmware. Choose **Factory** for the first USB installation and download the resulting file. The first build downloads additional tools, needs internet access, and can take several minutes.

## First Installation over USB

1. Connect the D1 mini to the computer running your browser using a data-capable USB cable.
2. Open [ESPHome Web](https://web.esphome.io/) in a browser that supports Web Serial, such as Chrome or Edge. Select **Connect** and choose the D1 mini's serial port.
3. Select **Install**, choose the downloaded factory binary, and complete the installation. Do not use the OTA binary for this first USB flash.
4. After flashing, connect the P1 interface and power the device. With the advanced firmware, complete [Advanced Firmware Setup](../README.md#advanced-firmware-setup). The standard firmware connects using the Wi-Fi credentials in `secrets.yaml`.

The browser accesses the USB device on the computer running the browser; it does not need to be attached to the Docker host or passed through to the container. If the serial port is unavailable, check the USB cable, drivers, permissions, and whether another program is using the port.

## Later Updates and Logs

Once the device is connected to Wi-Fi, select **Install** on the same dashboard card and choose the wireless option to compile and upload an OTA update. The Docker host must be able to reach the device. If name resolution fails, add `use_address: <device-ip>` under the existing `wifi:` section of the selected YAML, using the device's current IP address, and retry. A router DHCP reservation can keep that address stable. Alternatively, the advanced firmware's web interface accepts the OTA binary; see [Advanced Firmware Setup](../README.md#advanced-firmware-setup).

The advanced firmware's web interface presents the native Digest sign-in prompt directly. After signing in, use the top-right sign-out icon to revoke the session; the page attempts to close, but some browsers may show a signed-out confirmation instead.

Use the card's **Logs** action to inspect the running device. For [Home Assistant and MQTT](../README.md#home-assistant-and-mqtt) and [troubleshooting](../README.md#troubleshooting), see the project README. Both configurations enable API encryption with `p1_meter__encryption_key` and require `p1meter__ota_password` for ESPHome OTA updates. Restrict firmware updates to a trusted network.
