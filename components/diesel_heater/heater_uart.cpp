#include "heater_uart.h"
#include "esphome/core/log.h"
#include <cstring>

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
  if (periodic_send_ && tx_frame_valid_) {
    uint32_t now = millis();
    if (now - last_tx_time_ >= send_interval_ms_) {
      send_frame_();
      last_tx_time_ = now;
    }
  }
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
    memcpy(tx_frame_, data_, 24);
    tx_frame_valid_ = true;
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

uint16_t HeaterUart::crc16_modbus_(const uint8_t *data, uint16_t length) {
  static const uint16_t table[] = {
      0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
      0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
      0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
      0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
      0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
      0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
      0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
      0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
      0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
      0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
      0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
      0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
      0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
      0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
      0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
      0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
      0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
      0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
      0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
      0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
      0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
      0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
      0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
      0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
      0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
      0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
      0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
      0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
      0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
      0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
      0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
      0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040};

  uint16_t crc = 0xFFFF;
  while (length--) {
    uint8_t tmp = *data++ ^ crc;
    crc >>= 8;
    crc ^= table[tmp];
  }
  return crc;
}

void HeaterUart::send_frame_() {
  uint16_t crc = crc16_modbus_(tx_frame_, 22);
  tx_frame_[22] = (crc >> 8) & 0xFF;
  tx_frame_[23] = crc & 0xFF;
  write_array(tx_frame_, 24);
}

void HeaterUart::start() {
  if (!tx_frame_valid_)
    return;
  tx_frame_[2] = 0xA0;
  send_frame_();
  tx_frame_[2] = 0x00;
}

void HeaterUart::stop() {
  if (!tx_frame_valid_)
    return;
  tx_frame_[2] = 0x05;
  send_frame_();
  tx_frame_[2] = 0x00;
}

void HeaterUart::set_desired_temperature(uint8_t temperature) {
  if (!tx_frame_valid_)
    return;
  tx_frame_[4] = temperature;
  send_frame_();
}

void HeaterUart::set_initial_frame(const std::vector<uint8_t> &frame) {
  if (frame.size() != 24)
    return;
  memcpy(tx_frame_, frame.data(), 24);
  tx_frame_valid_ = true;
  periodic_send_ = true;
}

void HeaterUart::set_send_interval(uint32_t interval_ms) { send_interval_ms_ = interval_ms; }

}  // namespace heater_uart
}  // namespace esphome
