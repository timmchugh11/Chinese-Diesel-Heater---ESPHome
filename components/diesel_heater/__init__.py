import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import CONF_ID

# Define C++ namespace and class
heater_uart_ns = cg.esphome_ns.namespace("heater_uart")
HeaterUart = heater_uart_ns.class_("HeaterUart", cg.Component, uart.UARTDevice)

# Define config keys
CONF_UART_ID = "uart_id"
CONF_SET_TEMPERATURE = "set_temperature"
CONF_STATE = "state"
CONF_ERROR = "error"
CONF_MODE = "mode"
CONF_PUMP_FREQUENCY = "pump_frequency"
CONF_FAN_SPEED = "fan_speed"
CONF_CHAMBER_TEMPERATURE = "chamber_temperature"
CONF_DUTY_CYCLE = "duty_cycle"
CONF_ON_OFF = "on_off"
CONF_INITIAL_FRAME = "initial_frame"
CONF_SEND_INTERVAL = "send_interval"

# Schema definition
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HeaterUart),
    cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
    cv.Required(CONF_SET_TEMPERATURE): cv.use_id(sensor.Sensor),
    cv.Required(CONF_STATE): cv.use_id(sensor.Sensor),
    cv.Required(CONF_ERROR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_ON_OFF): cv.use_id(sensor.Sensor),
    cv.Required(CONF_PUMP_FREQUENCY): cv.use_id(sensor.Sensor),
    cv.Required(CONF_FAN_SPEED): cv.use_id(sensor.Sensor),
    cv.Required(CONF_CHAMBER_TEMPERATURE): cv.use_id(sensor.Sensor),
    cv.Required(CONF_DUTY_CYCLE): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_INITIAL_FRAME): cv.All(cv.ensure_list(cv.hex_uint8_t), cv.Length(min=24, max=24)),
    cv.Optional(CONF_SEND_INTERVAL, default="150ms"): cv.positive_time_period_milliseconds,
}).extend(cv.polling_component_schema("5s"))  # match PollingComponent(5000)

# Code generation
async def to_code(config):
    uart_var = await cg.get_variable(config[CONF_UART_ID])
    set_temp = await cg.get_variable(config[CONF_SET_TEMPERATURE])
    state = await cg.get_variable(config[CONF_STATE])
    error = await cg.get_variable(config[CONF_ERROR])
    on_off = await cg.get_variable(config[CONF_ON_OFF])
    pump = await cg.get_variable(config[CONF_PUMP_FREQUENCY])
    fan = await cg.get_variable(config[CONF_FAN_SPEED])
    chamber = await cg.get_variable(config[CONF_CHAMBER_TEMPERATURE])
    duty = await cg.get_variable(config[CONF_DUTY_CYCLE])

    var = cg.new_Pvariable(config[CONF_ID],
                           uart_var,
                           set_temp,
                           state,
                           error,
                           on_off,
                           pump,
                           fan,
                           chamber,
                           duty)

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)  # ✅ pass the whole config

    if CONF_INITIAL_FRAME in config:
        cg.add(var.set_initial_frame(config[CONF_INITIAL_FRAME]))
    if CONF_SEND_INTERVAL in config:
        cg.add(var.set_send_interval(config[CONF_SEND_INTERVAL].total_milliseconds))
