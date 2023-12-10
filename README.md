# Chinese-Diesel-Heater---ESPHome

ESPHome custom component that reads the data from the heater, its the first custom component I have made for esphome, so could probably be much better.

This connects directly to the blue wire (GPIO4) to read the data being transmitted from the controller and then the reply from the heater.

I hope to add control.

[Documentation used, credit Ray Jones for his work with these heaters](https://gitlab.com/mrjones.id.au/bluetoothheater/-/blob/master/Documentation/V9%20-%20Hacking%20the%20Chinese%20Diesel%20Heater%20Communications%20Protocol.pdf?ref_type=heads)


# Control Solution

I have struggled to get working commands sent over serial, so I connected 3 transistors to the switches on the controllers switchs, 'Power', 'Up' and 'Down' through 1k resistors to the ESP's GPIO so I can turn the heater on and off and control the duty through ESPHome.

![image](https://github.com/timmchugh11/Chinese-Diesel-Heater---ESPHome/assets/51882579/dbc770fe-6271-419e-b8ee-10471d517837)
```
1 - 5v
2 - Blue wire for reading data
3 - Down Button 5v
4 - Power Button 5v
5 - Up Button 5v
6 - Button 0v
8 - GND
```
```
switch:
  - platform: gpio
    name: "Power"
    pin: 12
  - platform: gpio
    name: "Up"
    pin: 13
  - platform: gpio
    name: "Down"
    pin: 14
```
