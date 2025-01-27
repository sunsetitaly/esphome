#include "sdp3x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sdp3x {

static const char *const TAG = "sdp3x.sensor";
static const uint8_t SDP3X_SOFT_RESET[2] = {0x00, 0x06};
static const uint8_t SDP3X_READ_ID1[2] = {0x36, 0x7C};
static const uint8_t SDP3X_READ_ID2[2] = {0xE1, 0x02};
static const uint8_t SDP3X_START_DP_AVG[2] = {0x36, 0x15};
static const uint8_t SDP3X_STOP_MEAS[2] = {0x3F, 0xF9};

void SDP3XComponent::update() { this->read_pressure_(); }

void SDP3XComponent::setup() {
  ESP_LOGD(TAG, "Setting up SDP3X...");

  if (this->write(SDP3X_STOP_MEAS, 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Stop SDP3X failed!");  // This sometimes fails for no good reason
  }

  if (this->write(SDP3X_SOFT_RESET, 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Soft Reset SDP3X failed!");  // This sometimes fails for no good reason
  }

  delayMicroseconds(20000);

  if (this->write(SDP3X_READ_ID1, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Read ID1 SDP3X failed!");
    this->mark_failed();
    return;
  }
  if (this->write(SDP3X_READ_ID2, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Read ID2 SDP3X failed!");
    this->mark_failed();
    return;
  }

  uint8_t data[18];
  if (this->read(data, 18) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Read ID SDP3X failed!");
    this->mark_failed();
    return;
  }

  if (!(check_crc_(&data[0], 2, data[2]) && check_crc_(&data[3], 2, data[5]))) {
    ESP_LOGE(TAG, "CRC ID SDP3X failed!");
    this->mark_failed();
    return;
  }

  if (data[3] == 0x01) {
    ESP_LOGCONFIG(TAG, "SDP3X is SDP31");
    pressure_scale_factor_ = 60.0f * 100.0f;  // Scale factors converted to hPa per count
  } else if (data[3] == 0x02) {
    ESP_LOGCONFIG(TAG, "SDP3X is SDP32");
    pressure_scale_factor_ = 240.0f * 100.0f;
  }

  if (this->write(SDP3X_START_DP_AVG, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Start Measurements SDP3X failed!");
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "SDP3X started!");
}
void SDP3XComponent::dump_config() {
  LOG_SENSOR("  ", "SDP3X", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Connection with SDP3X failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void SDP3XComponent::read_pressure_() {
  uint8_t data[9];
  if (this->read(data, 9) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Couldn't read SDP3X data!");
    this->status_set_warning();
    return;
  }

  if (!(check_crc_(&data[0], 2, data[2]) && check_crc_(&data[3], 2, data[5]) && check_crc_(&data[6], 2, data[8]))) {
    ESP_LOGW(TAG, "Invalid SDP3X data!");
    this->status_set_warning();
    return;
  }

  int16_t pressure_raw = encode_uint16(data[0], data[1]);
  float pressure = pressure_raw / pressure_scale_factor_;
  ESP_LOGV(TAG, "Got raw pressure=%d, scale factor =%.3f ", pressure_raw, pressure_scale_factor_);
  ESP_LOGD(TAG, "Got Pressure=%.3f hPa", pressure);

  this->publish_state(pressure);
  this->status_clear_warning();
}

float SDP3XComponent::get_setup_priority() const { return setup_priority::DATA; }

// Check CRC function from SDP3X sample code provided by sensirion
// Returns true if a checksum is OK
bool SDP3XComponent::check_crc_(const uint8_t data[], uint8_t size, uint8_t checksum) {
  uint8_t crc = 0xFF;

  // calculates 8-Bit checksum with given polynomial 0x31 (x^8 + x^5 + x^4 + 1)
  for (int i = 0; i < size; i++) {
    crc ^= (data[i]);
    for (uint8_t bit = 8; bit > 0; --bit) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x31;
      else
        crc = (crc << 1);
    }
  }

  // verify checksum
  return (crc == checksum);
}

}  // namespace sdp3x
}  // namespace esphome
