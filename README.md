# Chinese Diesel Heater ESPHome

An ESPHome external component for monitoring and controlling blue-wire Chinese
diesel heaters while retaining the original controller.

## Features

- Confirmed serial ON and OFF control over the blue-wire bus.
- A stateful Home Assistant power switch using serial commands.
- A native ESPHome Heater Duty number; no Home Assistant helper or duty
  automation is required.
- Set duty, heater state, error text, pump frequency, fan speed, chamber
  temperature and duty-cycle telemetry.
- Optional transistor-driven Up and Down controls for changing duty through
  the original controller.
- Optional transistor-driven Power control for backward compatibility.
- The original heater controller remains operational.

## How control works

Power ON and OFF are sent as 24-byte serial commands. After a CRC-valid
controller/heater exchange, the component copies the controller's current
24-byte packet, changes only its momentary command byte, and recalculates the
CRC. This preserves the controller's fixed/thermostatic mode, requested value,
pump and fan limits, voltage and controller-specific data. It waits a further
50 ms to match the known working protocol timing and then transmits the command.
A queued command expires after three seconds so an old ON request cannot run
much later.

The packet is copied at runtime rather than being taken from the example
installation. Both documented controller frame headers are accepted: `0x76`
from LCD/LED controllers and `0x78` from rotary controllers. The transmitted
command therefore retains the header and settings supplied by the connected
controller.

Run-state, error and derived power changes are published immediately when a
valid exchange reports a new value. Power is derived from the heater run-state
byte; response byte 3 is an error-state byte, not an ON/OFF flag. The brief
undocumented `0x10` value seen from some heaters during startup is ignored
until the next documented state arrives.

The original controller owns the persistent duty setting and sends it during
every exchange. A one-off serial duty value would be overwritten and would not
update the controller display. To change duty, two optional transistors
electronically press the original controller's Up and Down buttons. The
controller therefore remains the source of truth, its display stays correct,
and its physical buttons continue to work.

The optional `duty_control` number moves the controller from its reported duty
to the requested duty using the same fast burst behavior as the former Home
Assistant automation. It calculates the difference and sends the required
100 ms button pulses at a 200 ms cadence. It does not wait for feedback between
individual steps. After the burst, it waits five seconds for the controller to
apply the presses before confirming the result or sending a corrective burst.
When no target is active, changes made on the original controller update the
number in Home Assistant. The displayed number is updated on the normal
five-second interval only after the same duty value appears in two consecutive
packets, preventing brief start-up packet values from making the slider jump.

Serial power control and the legacy transistor Power control can be configured
at the same time. The transistor Power button is useful as a fallback, but do
not trigger both power methods simultaneously.

## Optional transistor controls

No transistors are required for telemetry or serial ON/OFF control. They are
only needed for the following optional functions:

| Function | Transistors required | What it does |
| --- | ---: | --- |
| Serial monitoring and ON/OFF | 0 | Uses only the shared blue-wire serial connection. |
| Duty control | 2 | Emulates the controller's Up and Down buttons. |
| Legacy Power control | 1 additional | Emulates a three-second press of the controller's Power button. |

Each transistor is wired across one controller button: the switched side joins
the button signal to the controller's button ground when its ESP32 GPIO is
active. This is an electronic button press; the ESP32 does not generate or
store the duty value itself. Use a suitable transistor interface and resistor,
and do not connect a controller button signal directly to an ESP32 GPIO.

For native duty control, the configuration has three related parts:

1. `up_gpio` and `down_gpio` are internal ESPHome GPIO switches that operate
   the two transistor interfaces. Because they are internal, raw transistor
   controls are not exposed to Home Assistant.
2. `up_switch: up_gpio` and `down_switch: down_gpio` tell the
   `diesel_heater` component which outputs to pulse.
3. `duty_control` creates the single `Heater Duty` number in Home Assistant.
   Moving it from 20 to 24 produces four Up pulses; moving it from 24 to 19
   produces five Down pulses. Telemetry then confirms the controller's result.

The Up and Down template buttons used by older configurations are not required
when `duty_control` is configured. The component operates the internal GPIO
switches directly. If duty control is not wanted, omit `duty_control`,
`up_switch`, `down_switch`, `up_gpio`, and `down_gpio` together.
The legacy Power transistor is independent; if it is not wanted, omit
`power_gpio` and `legacy_power_button`.

## Electrical requirements

The UART uses one physical GPIO for both RX and TX. TX must be open-drain so the
ESP32 cannot drive against another device on the shared bus. The bus also needs
an appropriate pull-up. The example explicitly configures open-drain mode for
ESP32 using ESP-IDF; do not replace the full pin definitions with bare GPIO
numbers.

The heater communication line may use a higher logic voltage than the ESP32.
Use appropriate input protection or level shifting for your heater/controller
hardware. Do not connect an unsafe voltage directly to an ESP32 GPIO.

## ESPHome configuration

This example uses GPIO33 for the blue-wire UART, GPIO9 for the optional legacy
Power transistor, GPIO11 for Up and GPIO7 for Down. Change the transistor GPIOs
to match your installation. It was tested on a Lolin S2 Mini with ESPHome
2026.7.0 and ESP-IDF.

```yaml
esp32:
  board: lolin_s2_mini
  framework:
    type: esp-idf

uart:
  id: heater_serial
  rx_pin:
    number: 33
    allow_other_uses: true
  tx_pin:
    number: 33
    mode:
      output: true
      open_drain: true
    allow_other_uses: true
  baud_rate: 25000

external_components:
  - source:
      type: git
      url: https://github.com/timmchugh11/Chinese-Diesel-Heater---ESPHome
      ref: main
    refresh: 1min

sensor:
  - platform: template
    id: set_temperature
    name: "Set Duty"
    accuracy_decimals: 0
    update_interval: never

  - platform: template
    id: pump_frequency
    name: "Pump Frequency"
    unit_of_measurement: "Hz"
    accuracy_decimals: 1
    update_interval: never

  - platform: template
    id: fan_speed
    name: "Fan Speed"
    unit_of_measurement: "RPM"
    accuracy_decimals: 0
    update_interval: never

  - platform: template
    id: chamber_temperature
    name: "Chamber Temperature"
    unit_of_measurement: "°C"
    accuracy_decimals: 0
    update_interval: never

  - platform: template
    id: duty_cycle
    name: "Duty Cycle"
    unit_of_measurement: "%"
    accuracy_decimals: 0
    update_interval: never

  - platform: template
    id: state_int
    internal: true
    accuracy_decimals: 0
    update_interval: never

  - platform: template
    id: error_int
    accuracy_decimals: 0
    internal: true
    update_interval: never

  - platform: template
    id: on_off
    internal: true
    accuracy_decimals: 0
    update_interval: never

text_sensor:
  - platform: template
    id: heater_state_text
    name: "Heater State"
    update_interval: never

  - platform: template
    id: heater_error_text
    name: "Heater Error"
    update_interval: never

diesel_heater:
  id: diesel_heater_uart
  uart_id: heater_serial
  # Optional: log every CRC-valid controller/heater exchange at DEBUG.
  dump_packets: false
  set_temperature: set_temperature
  state: state_int
  error: error_int
  state_text: heater_state_text
  error_text: heater_error_text
  on_off: on_off
  pump_frequency: pump_frequency
  fan_speed: fan_speed
  chamber_temperature: chamber_temperature
  duty_cycle: duty_cycle
  duty_control:
    name: "Heater Duty"
    mode: slider
  up_switch: up_gpio
  down_switch: down_gpio

switch:
  # Primary power control. Its displayed state follows serial telemetry rather
  # than assuming that a command succeeded.
  - platform: template
    name: "Heater Power"
    lambda: |-
      const int state = (int) id(state_int).state;
      return state >= 1 && state <= 5;
    turn_on_action:
      - lambda: |-
          id(diesel_heater_uart).request_on();
    turn_off_action:
      - lambda: |-
          id(diesel_heater_uart).request_off();

  # Optional internal transistor output for legacy Power-button control.
  - platform: gpio
    id: power_gpio
    pin: 9
    internal: true
    restore_mode: ALWAYS_OFF

  # Optional pair required by the native Heater Duty slider. These remain
  # internal: the diesel_heater component pulses them directly by ID.
  - platform: gpio
    id: up_gpio
    pin: 11
    internal: true
    restore_mode: ALWAYS_OFF

  - platform: gpio
    id: down_gpio
    pin: 7
    internal: true
    restore_mode: ALWAYS_OFF

button:
  # Optional backward-compatible transistor Power action. It is internal so
  # Home Assistant exposes only the serial Heater Power switch.
  - platform: template
    id: legacy_power_button
    name: "Heater Power Button (Legacy)"
    internal: true
    on_press:
      - switch.turn_on: power_gpio
      - delay: 3s
      - switch.turn_off: power_gpio
```

The serial power switch can take up to one telemetry update interval to reflect
the heater's new state. The command is queued and transmitted only once after a
CRC-valid bus exchange, and is discarded if no suitable exchange occurs within
three seconds.

## Replacing the Home Assistant duty automation

After flashing this configuration, use the new `number` entity named `Heater
Duty` directly. The old `input_number.heater_duty` helper and the `Diesel Heater
Duty` automation are no longer required and should be disabled or removed so
they do not send duplicate Up/Down presses.

## Controller connector reference

The connector numbering used by the original installation was:

```text
1 - 5 V
2 - Blue serial data wire
3 - Down button signal
4 - Power button signal
5 - Up button signal
6 - Button ground
8 - Ground
```

Verify your own controller before wiring because connector layouts can vary.
Use suitable transistor interfaces and base/gate resistors rather than
connecting ESP32 outputs directly across the controller buttons.

![Controller wiring](https://github.com/timmchugh11/Chinese-Diesel-Heater---ESPHome/assets/51882579/dbc770fe-6271-419e-b8ee-10471d517837)

## Logging

At `DEBUG` level, the component logs serial command requests, transmission,
completion and duty correction bursts. Normal 48-byte exchanges are not dumped
continuously unless packet dumping is explicitly enabled:

```yaml
logger:
  level: INFO
  logs:
    heater_uart: DEBUG

diesel_heater:
  # ...the required sensor references...
  dump_packets: true
```

With this enabled, every CRC-valid exchange is logged on one line as `RX
exchange [48 bytes]`. Serial commands generated by ESPHome are also logged as
`TX [24 bytes]`. Leave it disabled after troubleshooting to avoid excessive
logs.

## Credits

- [Ray Jones' Chinese diesel heater protocol documentation](https://gitlab.com/mrjones.id.au/bluetoothheater/-/blob/master/Documentation/V9%20-%20Hacking%20the%20Chinese%20Diesel%20Heater%20Communications%20Protocol.pdf?ref_type=heads)
- [PabloVitasso's wiring information](https://github.com/PabloVitasso/esphome-chinbasto/blob/main/README.md)
- Serial timing and command packets were based on wshelley's working Chinese
  Diesel Heater Advanced Temperature Controller implementation.
