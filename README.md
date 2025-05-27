# Chinese-Diesel-Heater---ESPHome

ESPHome external component that reads the data from the heater, its the first custom component I have made for esphome, so could probably be much better.

This connects directly to the blue wire (GPIO4) to read the data being transmitted from the controller and then the reply from the heater.

I hope to add control.

[Documentation used, credit Ray Jones for his work with these heaters](https://gitlab.com/mrjones.id.au/bluetoothheater/-/blob/master/Documentation/V9%20-%20Hacking%20the%20Chinese%20Diesel%20Heater%20Communications%20Protocol.pdf?ref_type=heads)

[Check out this readme from PabloVitasso for more detailed infomation about wiring](https://github.com/PabloVitasso/esphome-chinbasto/blob/main/README.md)


[Check out this readme from PabloVitasso for more detailed infomation about wiring](https://github.com/PabloVitasso/esphome-chinbasto/blob/main/README.md)


# Instructions

Simply wire up the esp to the controller, I try to keep the blue cable connection as short as possible for best results, then add the following to the yaml.

```
external_components:
  - source:
      type: git
      url: https://github.com/timmchugh11/Chinese-Diesel-Heater---ESPHome
      ref: main

sensor:
  - platform: template
    id: set_temperature
    name: "Set Duty"
    unit_of_measurement: ""
    lambda: "return {};"
    accuracy_decimals: 0

  - platform: template
    id: pump_frequency
    name: "Pump Frequency"
    unit_of_measurement: "Hz"
    lambda: "return {};"

  - platform: template
    id: fan_speed
    name: "Fan Speed"
    unit_of_measurement: "RPM"
    lambda: "return {};"
    accuracy_decimals: 0

  - platform: template
    id: chamber_temperature
    name: "Chamber Temperature"
    unit_of_measurement: "°C"
    lambda: "return {};"
    accuracy_decimals: 0

  - platform: template
    id: state_int
    name: "State (Int)"
    lambda: "return {};"
    internal: true
    accuracy_decimals: 0

  - platform: template
    id: error_int
    name: "Error (Int)"
    lambda: "return {};"
    accuracy_decimals: 0
    internal: true

  - platform: template
    id: duty_cycle
    name: "Duty Cycle"
    unit_of_measurement: "%"
    lambda: "return {};"
    accuracy_decimals: 0

  - platform: template
    id: on_off
    name: "Heater On/Off"
    lambda: "return {};"
    accuracy_decimals: 0
    internal: true

text_sensor:
  - platform: template
    id: state
    name: "State"
    update_interval: 5s
    lambda: |-
      if (id(error_int).state != 0 && id(error_int).state != 1) {
        if (id(error_int).state == 0) return optional<std::string>("No Error");
        else if (id(error_int).state == 1) return optional<std::string>("No Error, but started");
        else if (id(error_int).state == 2) return optional<std::string>("Voltage too low");
        else if (id(error_int).state == 3) return optional<std::string>("Voltage too high");
        else if (id(error_int).state == 4) return optional<std::string>("Ignition plug failure");
        else if (id(error_int).state == 5) return optional<std::string>("Pump Failure – over current");
        else if (id(error_int).state == 6) return optional<std::string>("Too hot");
        else if (id(error_int).state == 7) return optional<std::string>("Motor Failure");
        else if (id(error_int).state == 8) return optional<std::string>("Serial connection lost");
        else if (id(error_int).state == 9) return optional<std::string>("Gone-Out");
        else if (id(error_int).state == 10) return optional<std::string>("Temperature sensor failure");
        else if (id(error_int).state == 11) return optional<std::string>("Ignition fail");
        else if (id(error_int).state == 12) return optional<std::string>("Failed 1st ignite");
        else if (id(error_int).state == 13) return optional<std::string>("Excess fuel usage");
        else return optional<std::string>("Unknown");
      } else {
        if (id(state_int).state == 0) return optional<std::string>("Off");
        else if (id(state_int).state == 1) return optional<std::string>("Starting");
        else if (id(state_int).state == 2) return optional<std::string>("Pre-Heat");
        else if (id(state_int).state == 3) return optional<std::string>("Failed Start - Retrying");
        else if (id(state_int).state == 4) return optional<std::string>("Ignition - Now heating up");
        else if (id(state_int).state == 5) return optional<std::string>("Running Normally");
        else if (id(state_int).state == 6) return optional<std::string>("Stop Command Received");
        else if (id(state_int).state == 7) return optional<std::string>("Stopping");
        else if (id(state_int).state == 8) return optional<std::string>("Cooldown");
        else return optional<std::string>("Unknown");
      }


diesel_heater:
  uart_id: heater_serial
  set_temperature: set_temperature
  state: state_int
  error: error_int
  on_off: on_off
  pump_frequency: pump_frequency
  fan_speed: fan_speed
  chamber_temperature: chamber_temperature
  duty_cycle: duty_cycle
```

To add control via transistors you will need the following
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

# Control Solution

I have struggled to get working commands sent over serial, so I connected 3 transistors to the switches on the controller, 'Power', 'Up' and 'Down' through 1k resistors to the ESP's GPIO so I can turn the heater on and off and control the duty through ESPHome. The power switch needs to be on/'Pressed' for 3 seconds to turn the heater off.

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

