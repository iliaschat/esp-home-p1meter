# Compile and Install with VS Code

This guide uses the project's VS Code dev container. For ESPHome installed outside the container, see the [CLI guide](README-ESPHome-Python-CLI.md). For wiring, see the [project README](../README.md).

## Contents

- [Prepare the Dev Container](#prepare-the-dev-container)
- [VS Code Tasks and PlatformIO IDE](#vs-code-tasks-and-platformio-ide)
- [Compile and Flash](#compile-and-flash)
- [USB Flashing with WSL2](#usb-flashing-with-wsl2)
   - [Using the Device in the Dev Container](#using-the-device-in-the-dev-container)

## Prepare the Dev Container

1. Install [VS Code](https://code.visualstudio.com/), Docker, and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers). Get the project files by downloading the [project on GitHub](https://github.com/iliaschat/p1meter-dsmr-esphome) (**Code** > **Download ZIP**, then extract it) or by running `git clone https://github.com/iliaschat/p1meter-dsmr-esphome.git` if Git is installed. Then open the project folder in VS Code.
2. Select **Dev Containers: Reopen in Container** from the Command Palette. The container installs ESPHome and the ESPHome, PlatformIO, and C/C++ extensions. Internet access is needed to build the container and for the first firmware compilation, which downloads the ESP8266 toolchain.
3. The container creates `secrets.yaml` from `secrets-example.yaml` if needed. Follow [Credentials and Configuration](README-ESPHome-Python-CLI.md#credentials-and-configuration), then use the VS Code terminal. ESPHome is already installed; no virtual environment setup is needed.

## VS Code Tasks and PlatformIO IDE

You can build and upload without typing commands. Both options use the same firmware and board choices.

- **VS Code tasks:** select **Terminal** > **Run Task**, then a **P1 Meter** task. Each task asks for a firmware (`public` or `advanced`) and a board (`wemos-d1-mini`, `nodemcuv2`, `esp32-devkit`, `esp32-s2`, `esp32-s3`, or `esp32-c3`). The tasks are Compile firmware, Validate config, Upload firmware, Show device logs, and Clean.
- **PlatformIO IDE:** open the project folder. The root `platformio.ini` lists one environment per firmware and board, named `public-<board>` or `advanced-<board>`, for example `advanced-esp32-s3`. Choose an environment from the PlatformIO toolbar, then run its **P1 Meter** custom targets: compile, upload, logs, validate, and clean. The default environment is `public-wemos-d1-mini`.

The PlatformIO default build action does nothing. Use the **P1 Meter** compile target instead.

## Compile and Flash

Open the VS Code terminal inside the container and compile the chosen configuration:

```sh
esphome compile p1meter_wemosd1mini_standard.yaml
```

Both configurations require `p1_meter__encryption_key` in `secrets.yaml` for native API encryption and `p1meter__ota_password` for ESPHome OTA updates. The advanced configuration also requires `p1_meter__setup_wifi_password`; see [Credentials and Configuration](README-ESPHome-Python-CLI.md#credentials-and-configuration).

Use `p1meter_wemosd1mini_advanced.yaml` instead for the setup access point and web interface. For the first installation, connect the D1 mini with a data-capable USB cable and run:

```sh
esphome run p1meter_wemosd1mini_standard.yaml
```

Select the serial port if prompted. On Windows with WSL2, follow [USB Flashing with WSL2](#usb-flashing-with-wsl2) first. On other hosts, confirm that the serial device is visible in the container and that its permissions allow access. If needed, specify the actual port with `--device /dev/ttyUSB0`.

Docker Desktop on macOS cannot pass a USB serial device directly into the container. For the first USB installation on macOS, follow the [dashboard guide](README-ESPHome-Dashboard-Dockered.md) to build the firmware and flash it from the browser on your Mac.

After flashing, connect the P1 interface and power the device. With the advanced firmware, follow [Advanced Firmware Setup](../README.md#advanced-firmware-setup), and open `http://192.168.4.1/` in a browser while connected to `P1-Meter-Setup`. The setup UI uses the browser's native Digest sign-in prompt; the top-right sign-out icon revokes the session and attempts to close the page. For later Wi-Fi updates and logs, see the [ESPHome guide](README-ESPHome-Python-CLI.md#compile-and-flash); run its commands in the container terminal without activating a virtual environment. For [Home Assistant and MQTT](../README.md#home-assistant-and-mqtt) and [troubleshooting](../README.md#troubleshooting), see the project README.

## USB Flashing with WSL2

On Windows, install [usbipd-win](https://github.com/dorssel/usbipd-win/releases) (version 5 or later) and use an up-to-date WSL2 distribution. You can install it from PowerShell with `winget install --interactive --exact dorssel.usbipd-win`. Connect the D1 mini using a data-capable USB cable and keep a WSL terminal open while attaching it.

1. In Windows PowerShell, list the devices and identify the D1 mini's USB serial adapter and its bus ID:

   ```powershell
   usbipd list
   ```

2. In an **Administrator** PowerShell, share that device, substituting its bus ID:

   ```powershell
   usbipd bind --busid <busid>
   ```

   Binding normally persists across reboots; check its state with `usbipd list`.

3. In PowerShell, attach it to the running WSL2 instance and confirm its state:

   ```powershell
   usbipd attach --wsl --busid <busid>
   usbipd list
   ```

   Windows cannot use the device while it is attached to WSL. After unplugging it or restarting WSL, attach it again; its bus ID may have changed.

4. In the WSL terminal, locate the serial port and check its permissions:

   ```sh
   ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
   ```

   The actual port may differ from `/dev/ttyUSB0`. If access is denied, configure the WSL user's device permissions, such as membership in the device's owning group, and reopen the WSL session.

### Using the Device in the Dev Container

The dev container is preconfigured in `.devcontainer/devcontainer.json` with:

```json
"runArgs": ["--privileged", "-v", "/dev:/dev"]
```

The `.devcontainer/Dockerfile` also adds the `vscode` user to the `dialout` group. Mounting all of `/dev` means devices attached before or after the container starts are visible, including when the port is renumbered after a reset (for example `ttyUSB0` to `ttyUSB1`). `--privileged` grants the container full access to the host's devices, so use this only with a trusted project.

After changing these files, run **Dev Containers: Rebuild Container**; `runArgs` apply only when the container is created. Verify the port inside the container:

```sh
ls -l /dev/ttyUSB* /dev/ttyACM*
```

Then flash with `esphome run p1meter_wemosd1mini_standard.yaml --device /dev/ttyUSB0`, replacing the port as needed. If the port is missing, confirm that it is listed in WSL first (step 4); if it is not, re-run `usbipd attach`. If it is listed in WSL but not in the container, rebuild the container. When finished, release the device from PowerShell with `usbipd detach --busid <busid>`. See Microsoft's [WSL USB guide](https://learn.microsoft.com/en-us/windows/wsl/connect-usb) for troubleshooting.