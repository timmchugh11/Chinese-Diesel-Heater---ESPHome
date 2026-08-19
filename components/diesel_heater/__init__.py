import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, sensor, switch, text_sensor, uart
from esphome.const import CONF_ID

AUTO_LOAD = ["number", "text_sensor"]
DEPENDENCIES = ["uart"]

# Define C++ namespace and class
heater_uart_ns = cg.esphome_ns.namespace("heater_uart")
# The generated component ID can be referenced from template-button lambdas to
# call the public request_on() and request_off() experimental controls.
HeaterUart = heater_uart_ns.class_("HeaterUart", cg.Component, uart.UARTDevice)
HeaterDutyNumber = heater_uart_ns.class_("HeaterDutyNumber", number.Number)

# Define config keys
CONF_UART_ID = "uart_id"
CONF_SET_TEMPERATURE = "set_temperature"
CONF_STATE = "state"
CONF_ERROR = "error"
CONF_STATE_TEXT = "state_text"
CONF_ERROR_TEXT = "error_text"
CONF_MODE = "mode"
CONF_PUMP_FREQUENCY = "pump_frequency"
CONF_FAN_SPEED = "fan_speed"
CONF_CHAMBER_TEMPERATURE = "chamber_temperature"
CONF_DUTY_CYCLE = "duty_cycle"
CONF_ON_OFF = "on_off"
CONF_DUTY_CONTROL = "duty_control"
CONF_UP_SWITCH = "up_switch"
CONF_DOWN_SWITCH = "down_switch"
CONF_DUMP_PACKETS = "dump_packets"


def validate_duty_control(config):
    duty_keys = (CONF_DUTY_CONTROL, CONF_UP_SWITCH, CONF_DOWN_SWITCH)
    configured = [key for key in duty_keys if key in config]
    if configured and len(configured) != len(duty_keys):
        raise cv.Invalid(
            "duty_control, up_switch and down_switch must be configured together"
        )
    return config


# Schema definition
CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(HeaterUart),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Required(CONF_SET_TEMPERATURE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_STATE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_ERROR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_STATE_TEXT): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_ERROR_TEXT): cv.use_id(text_sensor.TextSensor),
        cv.Required(CONF_ON_OFF): cv.use_id(sensor.Sensor),
        cv.Required(CONF_PUMP_FREQUENCY): cv.use_id(sensor.Sensor),
        cv.Required(CONF_FAN_SPEED): cv.use_id(sensor.Sensor),
        cv.Required(CONF_CHAMBER_TEMPERATURE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_DUTY_CYCLE): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_DUTY_CONTROL): number.number_schema(HeaterDutyNumber),
        cv.Optional(CONF_UP_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_DOWN_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_DUMP_PACKETS, default=False): cv.boolean,
    }).extend(cv.polling_component_schema("5s")),
    validate_duty_control,
)

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
    await uart.register_uart_device(var, config)
    cg.add(var.set_dump_packets(config[CONF_DUMP_PACKETS]))

    if CONF_STATE_TEXT in config:
        state_text = await cg.get_variable(config[CONF_STATE_TEXT])
        cg.add(var.set_state_text_sensor(state_text))
    if CONF_ERROR_TEXT in config:
        error_text = await cg.get_variable(config[CONF_ERROR_TEXT])
        cg.add(var.set_error_text_sensor(error_text))

    if CONF_DUTY_CONTROL in config:
        up_switch = await cg.get_variable(config[CONF_UP_SWITCH])
        down_switch = await cg.get_variable(config[CONF_DOWN_SWITCH])
        duty_control = await number.new_number(
            config[CONF_DUTY_CONTROL],
            var,
            min_value=8,
            max_value=35,
            step=1,
        )
        cg.add(var.set_duty_control(duty_control, up_switch, down_switch))
