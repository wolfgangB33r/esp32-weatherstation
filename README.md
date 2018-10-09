# esp32-weatherstation
Esp32 wireless temperature and humidity sensor, measurements sent through MQTT to Home Assistant  

This project contains a simple esp32 based wireless sensor that measures temperature and humidity and transfers 
the measurments through MQTT protocol. The sensor is used along with a Home Assistant installation that contains a
default MQTT broker. 
The sensor also includes a configuration mode that requests the wireless network configuration through
a more than simple web page and stores the config in EEPROM.

See details within my blog on [Build a wireless MQTT temperature and humidity sensor for your Home Assistant](http://www.smartlab.at/build-a-wireless…r-home-assistant/)
