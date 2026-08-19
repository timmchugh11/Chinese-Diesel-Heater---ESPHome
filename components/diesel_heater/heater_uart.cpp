#include "heater_uart.h"
#include "esphome/core/log.h"

namespace esphome {
namespace heater_uart {

static const char *const TAG = "heater_uart";

static const uint32_t SERIAL_COMMAND_TIMEOUT_MS = 3000;
static const uint32_t DUTY_TARGET_TIMEOUT_MS = 60000;
static const uint32_t DUTY_PULSE_MS = 100;
static const uint32_t DUTY_FEEDBACK_DELAY_MS = 5000;
static const uint8_t DUTY_MAX_CORRECTION_ROUNDS = 5;

static uint16_t crc16_modbus(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

static bool frame_crc_valid(const uint8_t *frame) {
  const uint16_t crc = crc16_modbus(frame, 22);
  return frame[22] == static_cast<uint8_t>(crc >> 8) &&
         frame[23] == static_cast<uint8_t>(crc & 0xFF);
}

static const char *heater_state_text(int state) {
  switch (state) {
    case 0: return "Off";
    case 1: return "Starting";
    case 2: return "Pre-Heat";
    case 3: return "Failed Start - Retrying";
    case 4: return "Ignition - Heating Up";
    case 5: return "Running Normally";
    case 6: return "Stop Command Received";
    case 7: return "Stopping";
    case 8: return "Cooldown";
    default: return nullptr;
  }
}

static const char *heater_error_text(int error) {
  switch (error) {
    case 0: return "No Error";
    case 1: return "No Error - Started";
    case 2: return "Voltage Too Low";
    case 3: return "Voltage Too High";
    case 4: return "Ignition Plug Failure";
    case 5: return "Pump Overcurrent";
    case 6: return "Over Temperature";
    case 7: return "Motor Failure";
    case 8: return "Serial Connection Lost";
    case 9: return "Flame Out";
    case 10: return "Temperature Sensor Failure";
    case 11: return "Ignition Failed";
    case 12: return "First Ignition Attempt Failed";
    case 13: return "Excess Fuel Usage";
    default: return nullptr;
  }
}

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
  last_rx_byte_ms_ = millis();
  ESP_LOGD(TAG, "Setup complete");
}

void HeaterUart::request_on() {
  ESP_LOGD(TAG, "Serial ON requested");
  pending_command_ = PendingCommand::ON;
  pending_command_requested_ms_ = millis();
  ESP_LOGD(TAG, "Command queued waiting for next valid 48-byte exchange");
}

void HeaterUart::request_off() {
  ESP_LOGD(TAG, "Serial OFF requested");
  pending_command_ = PendingCommand::OFF;
  pending_command_requested_ms_ = millis();
  ESP_LOGD(TAG, "Command queued waiting for next valid 48-byte exchange");
}

void HeaterDutyNumber::control(float value) {
  parent_->request_duty(value);
}

void HeaterUart::set_duty_control(HeaterDutyNumber *duty_control,
                                  switch_::Switch *up_switch,
                                  switch_::Switch *down_switch) {
  duty_control_ = duty_control;
  up_switch_ = up_switch;
  down_switch_ = down_switch;
}

void HeaterUart::request_duty(float value) {
  int target = static_cast<int>(value + 0.5f);
  if (target < 8)
    target = 8;
  if (target > 35)
    target = 35;

  ESP_LOGD(TAG, "Duty target requested: %d", target);
  duty_target_ = target;
  duty_target_active_ = true;
  duty_target_requested_ms_ = millis();
  duty_correction_rounds_ = 0;
  duty_control_->publish_state(target);

  // A changed target stops the current burst. Wait for the next telemetry
  // value before calculating the replacement burst so pulses already accepted
  // by the controller are included.
  if (duty_pulse_state_ != DutyPulseState::IDLE ||
      duty_waiting_for_feedback_) {
    cancel_duty_burst_();
    duty_stable_candidate_ = -1;
    duty_stable_count_ = 0;
    duty_waiting_for_feedback_ = true;
    duty_feedback_wait_started_ms_ = millis();
    return;
  }

  duty_waiting_for_feedback_ = false;
  start_duty_burst_();
}

void HeaterUart::cancel_duty_burst_() {
  if (up_switch_ != nullptr)
    up_switch_->turn_off();
  if (down_switch_ != nullptr)
    down_switch_->turn_off();
  duty_pulse_state_ = DutyPulseState::IDLE;
  duty_pulses_remaining_ = 0;
}

void HeaterUart::start_duty_burst_() {
  if (!duty_target_active_ || duty_control_ == nullptr ||
      up_switch_ == nullptr || down_switch_ == nullptr)
    return;
  if (duty_stable_value_ < 8 || duty_stable_value_ > 35)
    return;

  const int current = duty_stable_value_;
  const int difference = duty_target_ - current;
  if (difference == 0) {
    duty_target_active_ = false;
    duty_waiting_for_feedback_ = false;
    duty_control_->publish_state(current);
    ESP_LOGD(TAG, "Duty target %d confirmed", current);
    return;
  }

  if (duty_correction_rounds_ >= DUTY_MAX_CORRECTION_ROUNDS ||
      millis() - duty_target_requested_ms_ > DUTY_TARGET_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Duty target %d not reached; controller remains at %d", duty_target_, current);
    duty_target_active_ = false;
    duty_waiting_for_feedback_ = false;
    duty_control_->publish_state(current);
    return;
  }

  duty_direction_up_ = difference > 0;
  duty_pulses_remaining_ = static_cast<uint8_t>(difference > 0 ? difference : -difference);
  duty_correction_rounds_++;
  duty_waiting_for_feedback_ = false;
  duty_pulse_state_ = DutyPulseState::HIGH;
  duty_pulse_phase_ms_ = millis();
  duty_stable_candidate_ = -1;
  duty_stable_count_ = 0;

  ESP_LOGD(TAG, "Duty correction burst: %u %s pulse(s)",
           duty_pulses_remaining_, duty_direction_up_ ? "UP" : "DOWN");
  if (duty_direction_up_)
    up_switch_->turn_on();
  else
    down_switch_->turn_on();
}

void HeaterUart::service_duty_pulse_() {
  if (duty_pulse_state_ == DutyPulseState::IDLE)
    return;

  const uint32_t now = millis();
  if (now - duty_pulse_phase_ms_ < DUTY_PULSE_MS)
    return;

  duty_pulse_phase_ms_ = now;
  if (duty_pulse_state_ == DutyPulseState::HIGH) {
    if (duty_direction_up_)
      up_switch_->turn_off();
    else
      down_switch_->turn_off();
    duty_pulse_state_ = DutyPulseState::LOW;
    return;
  }

  if (--duty_pulses_remaining_ == 0) {
    duty_pulse_state_ = DutyPulseState::IDLE;
    duty_waiting_for_feedback_ = true;
    duty_feedback_wait_started_ms_ = millis();
    duty_stable_candidate_ = -1;
    duty_stable_count_ = 0;
    return;
  }

  duty_pulse_state_ = DutyPulseState::HIGH;
  if (duty_direction_up_)
    up_switch_->turn_on();
  else
    down_switch_->turn_on();
}

void HeaterUart::handle_duty_telemetry_() {
  if (duty_control_ == nullptr || set_temp_val_ < 8 || set_temp_val_ > 35)
    return;

  const int current = static_cast<int>(set_temp_val_);

  if (current == duty_stable_candidate_) {
    if (duty_stable_count_ < 2)
      duty_stable_count_++;
  } else {
    duty_stable_candidate_ = current;
    duty_stable_count_ = 1;
  }
  if (duty_stable_count_ >= 2)
    duty_stable_value_ = current;

  if (!duty_target_active_) {
    return;
  }

  if (duty_stable_count_ < 2)
    return;

  // Let every requested burst finish, then give the controller the same five
  // seconds the former Home Assistant automation had before trusting its final
  // value. This also makes rapid slider changes wait for already-sent presses.
  if (duty_pulse_state_ != DutyPulseState::IDLE)
    return;
  if (duty_waiting_for_feedback_ &&
      millis() - duty_feedback_wait_started_ms_ < DUTY_FEEDBACK_DELAY_MS)
    return;

  const int stable_current = duty_stable_value_;
  if (stable_current == duty_target_) {
    cancel_duty_burst_();
    duty_target_active_ = false;
    duty_waiting_for_feedback_ = false;
    duty_control_->publish_state(stable_current);
    ESP_LOGD(TAG, "Duty target %d confirmed", stable_current);
    return;
  }

  // Match the former HA automation: the first burst is immediate, but wait for
  // the normal five-second telemetry interval before calculating a corrective
  // burst. The controller can otherwise still be applying the first presses.
  if (!duty_waiting_for_feedback_ && duty_correction_rounds_ == 0) {
    start_duty_burst_();
  } else if (duty_waiting_for_feedback_) {
    duty_waiting_for_feedback_ = false;
    start_duty_burst_();
  }
}

void HeaterUart::publish_discrete_telemetry_() {
  if (heater_state_ >= 0 && heater_state_ <= 8 && heater_state_ != published_state_) {
    heater_state_int_->publish_state(heater_state_);
    if (state_text_ != nullptr)
      state_text_->publish_state(heater_state_text(heater_state_));
    published_state_ = heater_state_;
  }

  if (heater_error_ >= 0 && heater_error_ <= 13 && heater_error_ != published_error_) {
    heater_error_int_->publish_state(heater_error_);
    if (error_text_ != nullptr)
      error_text_->publish_state(heater_error_text(heater_error_));
    published_error_ = heater_error_;
  }

  if (on_or_off_ >= 0 && on_or_off_ <= 1 && on_or_off_ != published_on_off_) {
    on_off_->publish_state(on_or_off_);
    published_on_off_ = on_or_off_;
  }
}

void HeaterUart::send_pending_command_() {
  if (pending_command_ == PendingCommand::NONE || !controller_frame_valid_)
    return;

  // Start with the controller's latest complete packet so its operating mode,
  // desired value, pump/fan limits, voltage and all controller-specific bytes
  // are preserved. Only the momentary command byte and CRC are changed.
  uint8_t command[24];
  for (size_t i = 0; i < sizeof(command); i++)
    command[i] = controller_frame_[i];
  command[2] = pending_command_ == PendingCommand::ON ? 0xA0 : 0x05;
  const uint16_t crc = crc16_modbus(command, 22);
  command[22] = static_cast<uint8_t>(crc >> 8);
  command[23] = static_cast<uint8_t>(crc & 0xFF);

  if (dump_packets_)
    ESP_LOGD(TAG, "TX [24 bytes]: %s",
             format_hex_pretty(command, sizeof(command)).c_str());

  ESP_LOGD(TAG, "Preserving controller demand %u and mode 0x%02X",
           static_cast<unsigned>(command[4]),
           static_cast<unsigned>(command[13]));

  // Match the timing of the known working experimental implementation.
  ESP_LOGD(TAG, "Waiting 50 ms before command transmission");
  delay(50);

  if (pending_command_ == PendingCommand::ON) {
    ESP_LOGD(TAG, "Sending serial ON command using current controller settings");
  } else {
    ESP_LOGD(TAG, "Sending serial OFF command using current controller settings");
  }
  write_array(command, sizeof(command));
  flush();

  // ESP-IDF receives the local TX on a shared RX/TX pin. The reference
  // half-duplex software UART does not feed that echo into its frame parser,
  // so discard at most the 24 bytes we just sent and immediately resume RX.
  size_t echo_bytes = 0;
  while (available() && echo_bytes < 24) {
    read();
    echo_bytes++;
  }

  // Never carry a partial pre-transmit frame into the next controller/heater
  // exchange. The next idle boundary will establish a clean synchronization
  // point for both parsers.
  rx_active_ = false;
  first_byte_received_ = false;
  count_ = 0;
  command_sync_active_ = false;
  command_sync_count_ = 0;
  last_rx_byte_ms_ = millis();

  if (echo_bytes > 0)
    ESP_LOGD(TAG, "Discarded %u local TX echo bytes", static_cast<unsigned>(echo_bytes));

  pending_command_ = PendingCommand::NONE;
  ESP_LOGD(TAG, "24-byte command transmission complete");
}

void HeaterUart::loop() {
  static const int DATA_LENGTH = 48;
  static const int SET_TEMP_INDEX = 4;
  static const int HEATER_STATE_INDEX = 26;
  static const int ACTIVE_ERROR_INDEX = 27;
  static const int HEATER_ERROR_INDEX = 41;
  static const int PUMP_FREQ_INDEX = 40;
  static const int FAN_SPEED_HIGH_INDEX = 30;
  static const int FAN_SPEED_LOW_INDEX = 31;
  static const int CHAMBER_TEMP_HIGH_INDEX = 34;
  static const int CHAMBER_TEMP_LOW_INDEX = 35;

  bool telemetry_exchange_complete = false;
  bool command_sync_exchange_complete = false;

  service_duty_pulse_();

  if (pending_command_ != PendingCommand::NONE &&
      millis() - pending_command_requested_ms_ > SERIAL_COMMAND_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Queued serial command expired before a valid exchange");
    pending_command_ = PendingCommand::NONE;
  }

  while (available()) {
    const uint32_t now = millis();
    const uint32_t rx_gap = now - last_rx_byte_ms_;
    last_rx_byte_ms_ = now;
    const uint8_t in_byte = static_cast<uint8_t>(read());

    // Keep command timing faithful to the working reference implementation.
    // This counter is deliberately independent of telemetry parsing: it only
    // identifies the 48-byte exchange after an idle gap and never publishes
    // values from that looser framing path.
    if (rx_gap > 100) {
      // A bus-idle boundary always starts a new exchange. Reset any incomplete
      // telemetry frame so a dropped byte cannot leave parsing stuck across
      // subsequent exchanges.
      rx_active_ = false;
      first_byte_received_ = false;
      count_ = 0;

      command_sync_active_ = true;
      command_sync_count_ = 0;
    }
    if (command_sync_active_) {
      command_sync_count_++;
      if (command_sync_count_ == DATA_LENGTH) {
        command_sync_active_ = false;
        command_sync_count_ = 0;
        command_sync_exchange_complete = true;
      }
    }

    if (!rx_active_) {
      // Preserve the original, reliable frame synchronization. A complete
      // exchange begins with a controller frame (0x76 for LCD/LED or 0x78 for
      // rotary controllers), followed by the heater's 0x76 response frame.
      if (!first_byte_received_) {
        if (in_byte == 0x76 || in_byte == 0x78) {
          data_[0] = in_byte;
          first_byte_received_ = true;
        }
      } else if (in_byte == 0x16) {
        data_[1] = 0x16;
        count_ = 2;
        rx_active_ = true;
        first_byte_received_ = false;
      } else {
        // Another start byte can itself be the beginning of the next header.
        if (in_byte == 0x76 || in_byte == 0x78) {
          data_[0] = in_byte;
          first_byte_received_ = true;
        } else {
          first_byte_received_ = false;
        }
      }
      continue;
    }

    data_[count_++] = in_byte;
    if (count_ == DATA_LENGTH) {
      rx_active_ = false;
      count_ = 0;
      first_byte_received_ = false;

      // Retain the validity checks used by the original telemetry parser so a
      // shifted or partial exchange cannot publish random sensor states.
      if ((data_[0] == 0x76 || data_[0] == 0x78) && data_[1] == 0x16 &&
          data_[24] == 0x76 && data_[25] == 0x16 &&
          data_[44] == 100 && data_[45] == 0 &&
          frame_crc_valid(data_) && frame_crc_valid(data_ + 24)) {
        telemetry_exchange_complete = true;
        // Process this exchange before consuming any following bus traffic.
        break;
      }
    }
  }

  if (telemetry_exchange_complete) {
    if (dump_packets_)
      ESP_LOGD(TAG, "RX exchange [48 bytes]: %s",
               format_hex_pretty(data_, DATA_LENGTH).c_str());

    for (size_t i = 0; i < sizeof(controller_frame_); i++)
      controller_frame_[i] = data_[i];
    controller_frame_valid_ = true;

    set_temp_val_ = data_[SET_TEMP_INDEX];
    const int received_state = data_[HEATER_STATE_INDEX];
    // Some heaters briefly emit undocumented state 0x10 while starting. Keep
    // the previous documented state until the next valid 0..8 state arrives.
    if (received_state >= 0 && received_state <= 8)
      heater_state_ = received_state;
    active_error_state_ = data_[ACTIVE_ERROR_INDEX];
    heater_error_ = data_[HEATER_ERROR_INDEX];
    // Response byte 3 is the active error state, not an ON/OFF flag. Derive
    // power from the documented run state instead.
    if (heater_state_ >= 0 && heater_state_ <= 8)
      on_or_off_ = heater_state_ >= 1 && heater_state_ <= 5 ? 1 : 0;
    pump_freq_val_ = data_[PUMP_FREQ_INDEX];
    fan_speed_val_ = data_[FAN_SPEED_HIGH_INDEX] * 256 + data_[FAN_SPEED_LOW_INDEX];
    chamber_temp_val_ = data_[CHAMBER_TEMP_HIGH_INDEX] * 256 + data_[CHAMBER_TEMP_LOW_INDEX];
    duty_cycle_val_ = ((set_temp_val_ - 8) / 27.0f) * 100.0f;
    publish_discrete_telemetry_();
    handle_duty_telemetry_();
  }

  if (command_sync_exchange_complete) {
    // Transmit, flush, and discard local one-wire echo before parsing resumes.
    send_pending_command_();
  }
}

void HeaterUart::update() {
  if (set_temp_val_ >= 8 && set_temp_val_ <= 35)
    set_temp_->publish_state(set_temp_val_);
  if (duty_cycle_val_ >= 0 && duty_cycle_val_ <= 100)
    duty_cycle_->publish_state(duty_cycle_val_);
  if (duty_control_ != nullptr && !duty_target_active_ &&
      duty_stable_value_ >= 8 && duty_stable_value_ <= 35)
    duty_control_->publish_state(duty_stable_value_);
  if ((pump_freq_val_ * 0.1f > 1.2f && pump_freq_val_ * 0.1f < 5.5f) || pump_freq_val_ == 0)
    pump_freq_->publish_state(pump_freq_val_ * 0.1f);
  if ((fan_speed_val_ >= 1000 && fan_speed_val_ <= 5500) || fan_speed_val_ == 0)
    fan_speed_->publish_state(fan_speed_val_);
  if (chamber_temp_val_ > 0 && chamber_temp_val_ < 230)
    chamber_temp_->publish_state(chamber_temp_val_);
}

}  // namespace heater_uart
}  // namespace esphome
