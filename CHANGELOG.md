# Changelog

## 0.1.0

- Reads your electricity and gas meter through its P1 port using a Wemos D1 mini.
- Shows electricity use and return per tariff, total and per phase power, current and voltage per phase, and gas use in Home Assistant.
- Shows power outages, short voltage dips and peaks on phase 1, meter ID, P1 version, and active tariff group.
- Standard version: connects to Home Assistant automatically. MQTT can be turned on or off with a switch.
- Advanced version: set up Wi-Fi and MQTT from a web page on the device's own setup network, without editing files.
- Advanced version: update firmware, factory reset, and reboot from the same web page.
- Updates can be installed over Wi-Fi. Reboot switch available in Home Assistant.
- Onboard LED shows Wi-Fi status.
