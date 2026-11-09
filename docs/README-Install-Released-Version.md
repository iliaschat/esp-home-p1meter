# Install the Released Version (No Coding Needed)

This guide installs the ready-made P1 Meter software on a Wemos D1 mini. You do not need to build anything or edit any files.

You need:

- A Wemos D1 mini
- A USB cable that can transfer data (not a charge-only cable)
- A computer with Google Chrome or Microsoft Edge
- An internet connection

## Step 1: Download the Software

1. Open the [project releases page](https://github.com/iliaschat/p1meter-dsmr-esphome/releases) in your browser.
2. Open the latest release.
3. Under **Assets**, click the file named `wemos-d1-mini-firmware.bin`. Your computer saves it, usually in your Downloads folder.

## Step 2: Connect the Wemos D1 Mini

1. Plug the Wemos D1 mini into your computer with the USB cable.
2. Do not connect it to the meter yet.

## Step 3: Install the Software

1. Open [web.esphome.io](https://web.esphome.io/) in Chrome or Edge.
2. Click **Connect**.
3. In the list, select the port for your Wemos D1 mini, then click **Connect**.
   - If no port appears, unplug the USB cable, try another USB port, and close any other program that uses the device.
4. Click **Install**.
5. Click **Choose file**, then select the `wemos-d1-mini-firmware.bin` file you downloaded.
6. Click **Install** again to confirm. Keep the device connected while the progress bar runs.
7. When the progress bar reaches 100%, click **Close**, unplug the USB cable, and plug it in again.

## Step 4: Set Up Wi-Fi

1. Connect the Wemos D1 mini to the P1 port of your meter, and power it on.
2. On your phone or computer, open the Wi-Fi settings and connect to the network named `P1-Meter-Setup`. The network has no password the first time.
3. Open a web browser and go to `http://192.168.4.1/`.
4. Sign in with:
   - Username: `p1meter`
   - Password: `p1meter-setup`
5. Click **Change setup password** and choose a new password of 8 to 63 characters. Write it down. The device restarts.
6. Reconnect to `P1-Meter-Setup` using your new password, then open `http://192.168.4.1/` again and sign in.
7. Click **Configure**.
   - Select your home Wi-Fi network and enter its password.
   - MQTT is optional. Leave it off if you do not use it.
   - Click **Save and reboot**.
8. The device joins your home Wi-Fi, and the `P1-Meter-Setup` network turns off. The setup is complete.

## Updates

To install a later version, download the newer file from the releases page. Open the device's IP address in a browser on your home network, sign in, click **OTA Update**, and select the file. Keep the power connected until the device restarts.

## Security

This version does not encrypt the connection to Home Assistant. Anyone on your network can read the meter values. Use it only on a trusted home network, and never make the device reachable from the internet.

For more details, see the [Software Usage Guide](README-Software-Usage-Guide.md).
