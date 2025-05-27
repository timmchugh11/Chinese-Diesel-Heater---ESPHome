#include "heater_uart.h"
#include "esphome/core/log.h"

namespace esphome {
namespace heater_uart {

static const char *const TAG = "heater_uart";

HeaterUart::HeaterUart(uart::UARTComponent *parent,
                       sensor::Sensor *set_temp,
                       sensor::Sensor *heater_state_int,
                       sensor::Sensor *heater_error_int,
                       sensor::Sensor *on_off,
                       sensor::Sensor *pump_freq,
                       sensor::Sensor *fan_speed,
                       sensor::Sensor *chamber_temp,
                       sensor::Sensor *duty_cycle)
    : PollingComponent(5000), UARTDevice(parent),
      set_temp_(set_temp),
      heater_state_int_(heater_state_int),
      heater_error_int_(heater_error_int),
      on_off_(on_off),
      pump_freq_(pump_freq),
      fan_speed_(fan_speed),
      chamber_temp_(chamber_temp),
      duty_cycle_(duty_cycle) {}

void HeaterUart::setup() {
  ESP_LOGD(TAG, "Setup complete");
}

void HeaterUart::loop() {
  static const int DATA_LENGTH = 48;
  static const int SET_TEMP_INDEX = 4;
  static const int HEATER_STATE_INDEX = 26;
  static const int HEATER_ERROR_INDEX = 41;
  static const int ON_OFF_INDEX = 27;
  static const int PUMP_FREQ_INDEX = 40;
  static const int FAN_SPEED_HIGH_INDEX = 30;
  static const int FAN_SPEED_LOW_INDEX = 31;
  static const int CHAMBER_TEMP_HIGH_INDEX = 34;
  static const int CHAMBER_TEMP_LOW_INDEX = 35;

  bool data_valid = false;

  while (available()) {
    int in_byte = read();

    if (rx_active_ && count_ < DATA_LENGTH) {
      data_[count_++] = in_byte;

      if (count_ == 48) {
        if (data_[45] == 0 && data_[44] == 100 && data_[24] == 0x76 && data_[42] == 0 && data_[25] == 0x16) {
          data_valid = true;
        }
      }
    } else if (in_byte == 0x76 && !first_byte_received_) {
      first_byte_received_ = true;
      data_[0] = in_byte;
      count_ = 1;
    } else if (in_byte == 0x16 && first_byte_received_) {
      second_byte_received_ = true;
      rx_active_ = true;
      data_[1] = in_byte;
      count_ = 2;
    } else {
      first_byte_received_ = false;
      second_byte_received_ = false;
    }
  }

  if (data_valid) {
    set_temp_val_ = data_[SET_TEMP_INDEX];
    heater_state_ = data_[HEATER_STATE_INDEX];
    heater_error_ = data_[HEATER_ERROR_INDEX];
    on_or_off_ = data_[ON_OFF_INDEX];
    pump_freq_val_ = data_[PUMP_FREQ_INDEX];
    fan_speed_val_ = data_[FAN_SPEED_HIGH_INDEX] * 256 + data_[FAN_SPEED_LOW_INDEX];
    chamber_temp_val_ = data_[CHAMBER_TEMP_HIGH_INDEX] * 256 + data_[CHAMBER_TEMP_LOW_INDEX];
    duty_cycle_val_ = ((set_temp_val_ - 8) / 27.0f) * 100.0f;

    rx_active_ = false;
    count_ = 0;
  }
}

void HeaterUart::update() {
  if (set_temp_val_ >= 8 && set_temp_val_ <= 35)
    set_temp_->publish_state(set_temp_val_);
  if (duty_cycle_val_ >= 0 && duty_cycle_val_ <= 100)
    duty_cycle_->publish_state(duty_cycle_val_);
  if (heater_state_ >= 0 && heater_state_ <= 8)
    heater_state_int_->publish_state(heater_state_);
  if (heater_error_ >= 0 && heater_error_ <= 13)
    heater_error_int_->publish_state(heater_error_);
  on_off_->publish_state(on_or_off_);
  if ((pump_freq_val_ * 0.1f > 1.2f && pump_freq_val_ * 0.1f < 5.5f) || pump_freq_val_ == 0)
    pump_freq_->publish_state(pump_freq_val_ * 0.1f);
  if ((fan_speed_val_ >= 1000 && fan_speed_val_ <= 5500) || fan_speed_val_ == 0)
    fan_speed_->publish_state(fan_speed_val_);
  if (chamber_temp_val_ > 0 && chamber_temp_val_ < 230)
    chamber_temp_->publish_state(chamber_temp_val_);
}

}  // namespace heater_uart
}  // namespace esphome
