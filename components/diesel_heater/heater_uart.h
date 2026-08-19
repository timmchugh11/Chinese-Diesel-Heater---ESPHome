#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace heater_uart {

class HeaterUart;

class HeaterDutyNumber : public number::Number {
 public:
  explicit HeaterDutyNumber(HeaterUart *parent) : parent_(parent) {}

 protected:
  void control(float value) override;
  HeaterUart *parent_;
};

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
  void dump_config() override {}

  void request_on();
  void request_off();
  void request_duty(float value);
  void set_dump_packets(bool dump_packets) { dump_packets_ = dump_packets; }
  void set_state_text_sensor(text_sensor::TextSensor *state_text) { state_text_ = state_text; }
  void set_error_text_sensor(text_sensor::TextSensor *error_text) { error_text_ = error_text; }
  void set_duty_control(HeaterDutyNumber *duty_control,
                        switch_::Switch *up_switch,
                        switch_::Switch *down_switch);

 protected:
  enum class PendingCommand : uint8_t { NONE, ON, OFF };
  enum class DutyPulseState : uint8_t { IDLE, HIGH, LOW };

  void send_pending_command_();
  void start_duty_burst_();
  void service_duty_pulse_();
  void handle_duty_telemetry_();
  void cancel_duty_burst_();
  void publish_discrete_telemetry_();

  sensor::Sensor *set_temp_;
  sensor::Sensor *heater_state_int_;
  sensor::Sensor *heater_error_int_;
  sensor::Sensor *on_off_;
  sensor::Sensor *pump_freq_;
  sensor::Sensor *fan_speed_;
  sensor::Sensor *chamber_temp_;
  sensor::Sensor *duty_cycle_;
  text_sensor::TextSensor *state_text_{nullptr};
  text_sensor::TextSensor *error_text_{nullptr};

  float current_temp_ = -1.0f;
  float set_temp_val_ = -1.0f;
  int heater_state_ = -1;
  int heater_error_ = -1;
  int active_error_state_ = -1;
  int on_or_off_ = -1;
  int published_state_ = -1;
  int published_error_ = -1;
  int published_on_off_ = -1;
  int pump_freq_val_ = -1;
  int fan_speed_val_ = -1;
  int chamber_temp_val_ = -1;
  float duty_cycle_val_ = -1.0f;
  int count_ = 0;
  bool rx_active_ = false;
  bool first_byte_received_ = false;
  uint32_t last_rx_byte_ms_ = 0;
  int command_sync_count_ = 0;
  bool command_sync_active_ = false;
  PendingCommand pending_command_ = PendingCommand::NONE;
  uint32_t pending_command_requested_ms_ = 0;

  HeaterDutyNumber *duty_control_{nullptr};
  switch_::Switch *up_switch_{nullptr};
  switch_::Switch *down_switch_{nullptr};
  int duty_target_ = -1;
  bool duty_target_active_ = false;
  bool duty_waiting_for_feedback_ = false;
  bool duty_direction_up_ = false;
  uint8_t duty_pulses_remaining_ = 0;
  uint8_t duty_correction_rounds_ = 0;
  uint8_t duty_stable_count_ = 0;
  int duty_stable_candidate_ = -1;
  int duty_stable_value_ = -1;
  DutyPulseState duty_pulse_state_ = DutyPulseState::IDLE;
  uint32_t duty_pulse_phase_ms_ = 0;
  uint32_t duty_feedback_wait_started_ms_ = 0;
  uint32_t duty_target_requested_ms_ = 0;
  bool controller_frame_valid_ = false;
  bool dump_packets_ = false;
  uint8_t controller_frame_[24];
  uint8_t data_[48];
};

}  // namespace heater_uart
}  // namespace esphome
