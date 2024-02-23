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
