# ePaper Info Display

A wireless, battery-powered ePaper Display that shows you important information at a glance.
Inspired by [a similiar project](https://spritesmods.com/?art=einkdisplay) by spritesmods.

A [7.4' ePaper Display from PervasiveDisplays](https://www.pervasivedisplays.com/product/7-4-e-ink-displays/) is used as the display.
It is powered by ESP32-C3 WiFi-enabled microcontroller.
The microcontroller wakes up every 24 hours and updates the e-paper display with an 800x480 image downloaded from a server.
On the server side, a Python script generates the image, showing a calendar, the weather forecast, and the next appointments.

This repository contains the hardware, firmware and software for the project.

## Hardware

The `hardware/` folder contains all schematics and PCB layouts in KiCAD format.

### Changelog

#### Rev. 3

- Fixed swapped USB DP+/-, USB usable again
- Fixed swapped labels USR1/USR2
- Add resistor divider for VBAT_SENSE to avoid excessive voltage on GPIO pin
- WAKEUP button changed to BOOT (connected to GPIO9)
- VBAT_SENSE moved to GPIO4 which has a usable ADC
- GPIO association of some signals changed to avoid issues strapping pins:
	- LED1 now on GPIO5
	- LED2 now on GPIO6
	- /BUSY now on GPIO7
	- /PWR_EN now on GPIO8

##### Known issues

None

#### Rev.2 (only usable with fixes)
- Use bottom-connect FPC connector
- Mirror some parts of the layout

##### Known issues
- USB DP+/- is switched
- #BUSY of Display is connected to GPIO9, which causes the chip to enter the Bootloader every time
- VBAT_SENSE is on ADC2_CH0 which is not usable when WiFi is active
- Labels USR1/USR2 are switched
- GPIO8 and GPIO2 (which are strapping pins) are used otherwise
- The WAKEUP button is useless (one can just use RESET), but a boot button would have been nice

#### Rev.1 (broken)
- First revision

##### Known issues
- Orientation/Location of ePaper FPC connector is wrong

## Firmware

The firmware can be found in the `firmware/` folder.
It is based on [ESPHome](https://esphome.io/).

To get started:
```shell
cd firmware/
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
cp secrets.example.yaml secrets.yaml
vim secrets.yaml
esphome compile epaperinfodisplay.yaml
esphome upload epaperinfodisplay.yaml
```

There is a ESPHome-compatible custom component for the e-paper display from Pervasive Displays in `esphome/`.
This might also be useful outside this project.

## Software

The host software is writting in Python using [Flask](https://flask.palletsprojects.com/en/stable/).
It must run on some kind of server, like a Raspberry Pi or a NAS.

To get started:
```shell
cd software/
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
cp config.example.yaml config.yaml
vim config.yaml
flask run --host 0.0.0.0 # make accessible from outside localhost
```

Weather data is fetched from [OpenMeteo](https://open-meteo.com/).
For this to work you need to specify your location in the `config.yaml` file.
Calendar appointments are fetched from a CalDAV server.
The example configuration contains the correct path for [Baïkal](https://sabre.io/baikal/), but any kind of CalDAV server should work.
You might need to find out the correct path for your server.

## License

MIT
