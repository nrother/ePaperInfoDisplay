#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace pervasive_displays_epaper {

class PervasiveDisplaysEPaperBase : public display::DisplayBuffer,
                            public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                  spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void set_busy_pin(GPIOPin *busy_pin) { this->busy_pin_ = busy_pin; }
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_enable_pin(GPIOPin *enable_pin) { this->enable_pin_ = enable_pin; }
  void set_temperature(float temp) { this->temperature_ = temp; }
  float get_setup_priority() const override;

  virtual void display() = 0;

  void update() override;

  void setup() override {
    this->setup_pins_();
  }

 protected:
  void setup_pins_();
  virtual uint32_t get_buffer_length_() = 0;
  
  GPIOPin *busy_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *enable_pin_{nullptr};
  float temperature_;
};

class PervasiveDisplaysEPaper : public PervasiveDisplaysEPaperBase {
 public:
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

  void display() override;
  void dump_config() override;

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  uint32_t get_buffer_length_() override;
  virtual void dump_config_model_() = 0;
  void power_on_COG_();
  void DCDC_soft_start_mid_();
  void DCDC_soft_shutdown_mid_();
  void send_index_data_(uint8_t index, const uint8_t *data, uint32_t len);

  virtual void send_duw_drfw_() = 0;
  virtual void send_ram_rw_() = 0;
};

class PervasiveDisplaysEPaper581In : public PervasiveDisplaysEPaper {
 protected:
  int get_width_internal() override;
  int get_height_internal() override;
  void dump_config_model_() override;
  void send_duw_drfw_() override;
  void send_ram_rw_() override;
};

class PervasiveDisplaysEPaper741In : public PervasiveDisplaysEPaper {
 protected:
  int get_width_internal() override;
  int get_height_internal() override;
  void dump_config_model_() override;
  void send_duw_drfw_() override;
  void send_ram_rw_() override;
};

}
}