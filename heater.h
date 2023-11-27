#include "esphome.h"

class HeaterUart : public PollingComponent, public UARTDevice {  public: 
  Sensor *set_temp = new Sensor();
  Sensor *heater_state_int = new Sensor();
  Sensor *heater_error_int = new Sensor();
  Sensor *on_off = new Sensor();
  Sensor *pump_freq = new Sensor();
  Sensor *fan_speed = new Sensor();
  Sensor *chamber_temp = new Sensor();
  Sensor *duty_cycle = new Sensor();
  public:
    HeaterUart(UARTComponent *parent, Sensor *set_temp, Sensor *heater_state_int, Sensor *heater_error_int, Sensor *on_off, Sensor *pump_freq, Sensor *fan_speed, Sensor *chamber_temp, Sensor *duty_cycle) : PollingComponent(5000), UARTDevice(parent) {
      this->set_temp = set_temp;
      this->heater_state_int = heater_state_int;
      this->heater_error_int = heater_error_int;
      this->on_off = on_off;
      this->pump_freq = pump_freq;
      this->fan_speed = fan_speed;
      this->chamber_temp = chamber_temp;
      this->duty_cycle = duty_cycle;
    }

    void setup() override {
      ESP_LOGD("heater_temp_module", "Setup");
    }

    void loop() override {
        
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
        bool dataValid = false;
        if (available()) {
            int inByte = read();
            if (RxActive && count < DATA_LENGTH) {
                data[count] = inByte;
                count++;
                if (count == 48) {
                    if (data[45] == 0 && data[44] == 100 && data[42] == 0 && data[25] == 0x16) {
                        ESP_LOGI("heater_temp_module", "Got all data, Parsed Data: Set Temp: %d, Heater State: %d, Heater Error: %d, On/Off: %d, Pump Freq: %d, Fan Speed: %d, Chamber Temp: %d", data[SET_TEMP_INDEX], data[HEATER_STATE_INDEX], data[HEATER_ERROR_INDEX], data[ON_OFF_INDEX], data[PUMP_FREQ_INDEX], data[FAN_SPEED_HIGH_INDEX] * 256 + data[FAN_SPEED_LOW_INDEX], data[CHAMBER_TEMP_HIGH_INDEX] * 256 + data[CHAMBER_TEMP_LOW_INDEX]);
                        dataValid = true;
                    } else {
                        dataValid = false;
                    };
                }
            } else if (inByte == 0x76 && !firstByteReceived) {
                firstByteReceived = true;
                data[0] = inByte;
                count = 1;
            } else if (inByte == 0x16 && firstByteReceived) {
                secondByteReceived = true;
                RxActive = true;
                data[1] = inByte;
                count = 2;
            } else {
                firstByteReceived = false;
                secondByteReceived = false;
            }
        }

        if (dataValid) {
            setTemp = data[SET_TEMP_INDEX];
            heaterState = int(data[HEATER_STATE_INDEX]);
            heaterError = int(data[HEATER_ERROR_INDEX]);
            onOrOff = data[ON_OFF_INDEX];
            pumpFreq = data[PUMP_FREQ_INDEX];
            fanSpeed = data[FAN_SPEED_HIGH_INDEX] * 256 + data[FAN_SPEED_LOW_INDEX];
            chamberTemp = data[CHAMBER_TEMP_HIGH_INDEX] * 256 + data[CHAMBER_TEMP_LOW_INDEX];
            dutyCycle = (((setTemp - 8) / 27) * 100);
            RxActive = false;
            count = 0;
        }
    };


    void update() override {
            if (setTemp >= 8 && setTemp <= 35){
                set_temp->publish_state(setTemp);
            } else {
                ESP_LOGE("heater_temp_module", "Set temperature out of range: %d", setTemp);
            }
            if (dutyCycle >= 0 && dutyCycle <= 100){
                duty_cycle->publish_state(dutyCycle);
            } else {
                ESP_LOGE("heater_temp_module", "Duty cycle out of range: %d", dutyCycle);
            }
            if (heaterState >= 0 && heaterState <= 8){
                heater_state_int->publish_state(heaterState);
            } else {
                ESP_LOGE("heater_temp_module", "Heater state out of range: %d", heaterState);
            }
            if (heaterError >= 0 && heaterError <= 13){
                heater_error_int->publish_state(heaterError);
            } else {
                ESP_LOGE("heater_temp_module", "Heater error out of range: %d", heaterError);
            }
            on_off->publish_state(onOrOff);
            if (((pumpFreq * 0.1) > 1.2 && (pumpFreq * 0.1) < 6) || (pumpFreq == 0)){
                pump_freq->publish_state(pumpFreq * 0.1);
            } else {
                ESP_LOGE("heater_temp_module", "Pump frequency out of range: %d", pumpFreq);
            }
            if ((fanSpeed >= 1000 && fanSpeed <= 5000) || (fanSpeed == 0)){
                fan_speed->publish_state(fanSpeed);
            } else {
                ESP_LOGE("heater_temp_module", "Fan speed out of range: %d", fanSpeed);
            }
            if (chamberTemp > 0 && chamberTemp < 230){
                chamber_temp->publish_state(chamberTemp);
            } else {
                ESP_LOGE("heater_temp_module", "Chamber temperature out of range: %d", chamberTemp);
            }
        }

    float currentTemp;
    float setTemp;
    int heaterState;
    int heaterError;
    int onOrOff;
    int pumpFreq;
    int fanSpeed;
    int chamberTemp;
    int count = 0;
    bool RxActive = false;
    byte data[48];
    float dutyCycle;
    bool firstByteReceived = false;
    bool secondByteReceived = false;
  };