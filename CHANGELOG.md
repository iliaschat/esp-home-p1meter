# Changelog

## 0.2.0

- Removed the footer text from the setup home page.
- Added a simplified installation guide for released firmware that needs no coding.
- Added a software usage guide for the web interface.
- Reorganized the documentation.
- The example `secrets-example.yaml` now uses a placeholder encryption key. Generate your own with `openssl rand -base64 32`.
- The build script now links `secrets.yaml` into each device folder, so `!secret` values resolve when building.
- Updated the VS Code setup and the release workflow.

## 0.1.0

- Reads your electricity and gas meter through its P1 port using a Wemos D1 mini.
- Shows electricity use and return per tariff, total and per phase power, current and voltage per phase, and gas use in Home Assistant.
- Shows power outages, short voltage dips and peaks on phase 1, meter ID, P1 version, and active tariff group.
- Standard version: connects to Home Assistant automatically. MQTT can be turned on or off with a switch.
- Advanced version: set up Wi-Fi and MQTT from a web page on the device's own setup network, without editing files.
- Advanced version: update firmware, factory reset, and reboot from the same web page.
- Updates can be installed over Wi-Fi. Reboot switch available in Home Assistant.
- Onboard LED shows Wi-Fi status.
