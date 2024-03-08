# E-paper weather station

Here you'll find all the files for my weather station project.

This project consists of three parts:
- A "Sender", which is a battery powered probe that sits outside and collects temperature, humidity, and atmospheric pressure data every 5 minutes
- A "Rain Probe", which sits inside, collects rainfall data every half hour, 
- A "Reciever", which sits inside, has a screen to show the data on, and processes and shows all the data.

This Repository ( and project for that matter ) is still a WIP, and will get updated over time.
## Features:
- A 4.2" E-paper screen
- Small form factor
- runs on an ESP32
- Custom PCB's for both the sender and the reciever, as well as 3d printable enclosures
- Temperature, humidity, air pressure, dew point and rainfall readings
- Daily minimum/maximum readings that reset at 0:00
- Auto-ranging 12 hour graphs of temperature, humidity and air pressure
- 12 hour bar graph for rainfall
- Built-in Zambretti forecast
##  Bill of Materials

I buy most of my parts from [Tinytronics](https://www.tinytronics.nl/en), a local store. They ship to what seems like most of Europe, though they're definitely  not the cheapest.
I will link the products I bought from their site, but feel free to buy them elsewhere.

### Reciever:
- 4.2" [Black/white](https://www.waveshare.com/4.2inch-e-paper-module.htm) or [black/white/red](https://www.waveshare.com/product/4.2inch-e-paper-module-b.htm) E-paper display
- [A DS3231 RTC module](https://www.tinytronics.nl/en/sensors/time/dfrobot-fermion-ds3232-rtc-module-i2c)
- [An ESP32 devboard](https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-wi-fi/esp32-wifi-and-bluetooth-board-cp2102), there exist a couple different types of these, be sure to check the pinout!
- [A HC-12 433Mhz wireless transiever](https://www.tinytronics.nl/en/communication-and-signals/wireless/rf/modules/hc-12-si4438-wireless-serial-port-module-433mhz) (Be sure to check your local laws on transmitting on this band!)
- [Female header pins](https://www.tinytronics.nl/en/cables-and-connectors/connectors/pin-headers/female/20-pins-header-female)
- [My custom PCB](https://github.com/Snow014/Epaper_WeatherStation/tree/Reciever/Reciever/PCB)


### Reciever:
- [A LilyGO T-base](https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-wi-fi/lilygo-ttgo-t-base-esp8266)
- [A HC-12 433Mhz wireless transiever](https://www.tinytronics.nl/en/communication-and-signals/wireless/rf/modules/hc-12-si4438-wireless-serial-port-module-433mhz) (Be sure to check your local laws on transmitting on this band!)
- [A BME280](https://www.tinytronics.nl/en/sensors/air/pressure/bme280-digital-barometer-pressure-and-humidity-sensor-module)
- [A SHT40](https://www.tinytronics.nl/en/sensors/air/humidity/dfrobot-fermion-sht40-temperature-and-humidity-sensor)
- [A LiPo battery](https://www.tinytronics.nl/en/power/batteries/li-po/li-po-accu-3.7v-2000mah) (Always be careful with LiPo batteries!)
- 


### Rain probe