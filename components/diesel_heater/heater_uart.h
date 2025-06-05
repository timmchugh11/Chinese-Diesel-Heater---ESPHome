#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace heater_uart {

class HeaterUart : public PollingComponent, public uart::UARTDevice {
 public:
  HeaterUart(uart::UARTComponent *parent,
             sensor::Sensor *set_temp,
             sensor::Sensor *heater_state_int,
             sensor::Sensor *heater_error_int,
             sensor::Sensor *on_off,
             sensor::Sensor *pump_freq,
             sensor::Sensor *fan_speed,
             sensor::Sensor *chamber_temp,
             sensor::Sensor *duty_cycle);

  void setup() override;
  void loop() override;
  void update() override;
  void start();
  void stop();
  void set_desired_temperature(uint8_t temperature);
  void set_initial_frame(const std::vector<uint8_t> &frame);
  void set_send_interval(uint32_t interval_ms);
  void dump_config() override {}

 protected:
  sensor::Sensor *set_temp_;
  sensor::Sensor *heater_state_int_;
  sensor::Sensor *heater_error_int_;
  sensor::Sensor *on_off_;
  sensor::Sensor *pump_freq_;
  sensor::Sensor *fan_speed_;
  sensor::Sensor *chamber_temp_;
  sensor::Sensor *duty_cycle_;

  float current_temp_;
  float set_temp_val_;
  int heater_state_;
  int heater_error_;
  int on_or_off_;
  int pump_freq_val_;
  int fan_speed_val_;
  int chamber_temp_val_;
  float duty_cycle_val_;
  int count_ = 0;
  bool rx_active_ = false;
  bool first_byte_received_ = false;
  bool second_byte_received_ = false;
  uint8_t data_[48];
  uint8_t tx_frame_[24];
  bool tx_frame_valid_ = false;
  bool periodic_send_ = false;
  uint32_t send_interval_ms_ = 150;
  uint32_t last_tx_time_ = 0;

  uint16_t crc16_modbus_(const uint8_t *data, uint16_t length);
  void send_frame_();
};

}  // namespace heater_uart
}  // namespace esphome
