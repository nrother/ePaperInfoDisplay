#include "pervasive_displays_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace pervasive_displays_epaper {

static const char *const TAG = "pervasive_displays_epaper";

void PervasiveDisplaysEPaperBase::setup_pins_() {
  this->init_internal_(this->get_buffer_length_());
  this->busy_pin_->setup();
  this->dc_pin_->setup();
  this->dc_pin_->digital_write(false);
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);
  this->enable_pin_->setup();
  this->enable_pin_->digital_write(true); //Power off

  this->spi_setup();
  this->cs_->digital_write(false); // make sure all pins are low when power is off
}

float PervasiveDisplaysEPaperBase::get_setup_priority() const { return setup_priority::AFTER_WIFI; } //TODO: Change to processor

void PervasiveDisplaysEPaperBase::update() {
  this->do_update_();
  this->display();
}

//note: the following code is inspired from PervasiveDisplays sample Arduino code
//TODO: There are a lot of hardcoded values here. According to the app note these should be read from OTP

void PervasiveDisplaysEPaper::display() {
  this->power_on_COG_();

	uint8_t dtcl = 0x08; // 0=IST7232, 8=IST7236
	this->send_index_data_(0x01, &dtcl, 1); //DCTL 0x10 of OTP

	// Send image data
	this->send_duw_drfw_();
  this->send_ram_rw_();

	// send first frame
  this->send_index_data_(0x10, this->buffer_, this->get_buffer_length_()); // First frame

	this->send_ram_rw_();

	// send second frame
  // there is no second from for B/W displays, just send the same data again, it is ignored anyway
  this->send_index_data_(0x11, this->buffer_, this->get_buffer_length_()); // Second frame

  this->DCDC_soft_start_mid_();

	while (this->busy_pin_->digital_read() != true) {
		delay(10);
    App.feed_wdt();
	}
  uint8_t data18[] = {0x3c};
	this->send_index_data_(0x15, data18, 1); //Display Refresh

  ESP_LOGD(TAG, "dcdc shutdown");
	this->DCDC_soft_shutdown_mid_();
}

void HOT PervasiveDisplaysEPaper::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t pos = (x + y * this->get_width_internal()) / 8u;
  const uint8_t subpos = x & 0x07;
  if (color.is_on()) {
    this->buffer_[pos] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos] &= ~(0x80 >> subpos);
  }
}

uint32_t PervasiveDisplaysEPaper::get_buffer_length_() {
  return this->get_width_internal() * this->get_height_internal() / 8u;
}

void PervasiveDisplaysEPaper::power_on_COG_() {
  this->enable_pin_->digital_write(false); //turn on power
	delay(200); //wait for VCC to be stable
  this->reset_pin_->digital_write(true); // RES# = 1
  delay(2);
  this->reset_pin_->digital_write(false);
  delay(4);
  this->reset_pin_->digital_write(true);
  delay(20);
  this->cs_->digital_write(true); // CS# = 1
}

void PervasiveDisplaysEPaper::DCDC_soft_start_mid_() {
  // COG init
  uint8_t data4[] = {0x7d};
  this->send_index_data_(0x05, data4, 1);
  delay(50);
  uint8_t zero[] = {0x00};
  this->send_index_data_(0x05, zero, 1);
  delay(1);
  //uint8_t data6[] = {0x3f}; //TODO: This is not in the app node
  //this->send_index_data_(0xc2, data6, 1);
  //delay(1);
  uint8_t ms_sync[] = {0x00};
  this->send_index_data_(0xd8, ms_sync, 1); // MS_SYNC 0x1D of OTP
  uint8_t bvss[] = {0x00};
  this->send_index_data_(0xd6, bvss, 1); // BVSS 0x1E of OTP
  uint8_t data9[] = {0x10};
  this->send_index_data_(0xa7, data9 , 1);
  delay(2);
  this->send_index_data_(0xa7, zero, 1);
  delay(10);
  //uint8_t data10[] = {0x00, 0x01}; //TODO: This is not in the app node
  //this->send_index_data_(0x03, data10, 2); // OSC mtp_0x12
  this->send_index_data_(0x44, zero, 1);
  uint8_t data11[] = {0x80};
  this->send_index_data_(0x45, data11, 1);
  this->send_index_data_(0xa7, data9, 1);
  delay(2);
  this->send_index_data_(0xa7, zero, 1);
  delay(10);
  uint8_t data12[] = {0x06};
  this->send_index_data_(0x44, data12, 1);
  uint8_t temp_data;
  if (this->temperature_ <= -40) {
    temp_data = 0x00;
  } else if (this->temperature_ >= 87) {
    temp_data = 0xFE;
  } else {
    temp_data = (int)((this->temperature_ + 40) * 2);
  }
  uint8_t data13[] = {temp_data};
  this->send_index_data_(0x45, data13, 1); // Temperature
  this->send_index_data_(0xa7, data9, 1);
  delay(2);
  this->send_index_data_(0xa7, zero, 1);
  delay(10);
  uint8_t tcon[] = {0x25};
  this->send_index_data_(0x60, tcon, 1); // TCON 0x0B of OTP
  uint8_t stv_dir[] = {0x00};
  this->send_index_data_(0x61, stv_dir, 1); // STV_DIR 0x1B of OTP
  //uint8_t data16[] = {0x00}; //TODO: This is not in the app node
  //this->send_index_data_(0x01, data16, 1); // DCTL mtp_0x10
  uint8_t vcom[] = {0x00};
  this->send_index_data_(0x02, vcom, 1); // VCOM 0x11 of OTP

  // DC-DC soft-start
  uint8_t index51[] = {0x50, 0x01, 0x0a, 0x01};
  this->send_index_data_(0x51, &index51[0], 2);
  uint8_t index09[] = {0x1f, 0x9f, 0x7f, 0xff};

  for (int value = 1; value <= 4; value++)
  {
      this->send_index_data_(0x09, &index09[0], 1);
      index51[1] = value;
      this->send_index_data_(0x51, &index51[0], 2);
      this->send_index_data_(0x09, &index09[1], 1);
      delay(2);
  }
  for (int value = 1; value <= 10; value++)
  {
      this->send_index_data_(0x09, &index09[0], 1);
      index51[3] = value;
      this->send_index_data_(0x51, &index51[2], 2);
      this->send_index_data_(0x09, &index09[1], 1);
      delay(2);
  }
  for (int value = 3; value <= 10; value++)
  {
      this->send_index_data_(0x09, &index09[2], 1);
      index51[3] = value;
      this->send_index_data_(0x51, &index51[2], 2);
      this->send_index_data_(0x09, &index09[3], 1);
      delay(2);
  }
  for (int value = 9; value >= 2; value--)
  {
      this->send_index_data_(0x09, &index09[2], 1);
      index51[2] = value;
      this->send_index_data_(0x51, &index51[2], 2);
      this->send_index_data_(0x09, &index09[3], 1);
      delay(2);
  }
  this->send_index_data_(0x09, &index09[3], 1);
  delay(10);
}

void PervasiveDisplaysEPaper::DCDC_soft_shutdown_mid_() {
  // DC-DC off
  while (this->busy_pin_->digital_read() != true) {
    delay(10);
    App.feed_wdt();
  }
  uint8_t data19[] = {0x7f};
  this->send_index_data_(0x09, data19, 1);
  uint8_t data20[] = {0x7d}; //TODO: App note has 0x3D
  this->send_index_data_(0x05, data20, 1);
  //TODO: App note write 0x09 0x7a and wait 15ms
  uint8_t data55[] = {0x00};
  this->send_index_data_(0x09, data55, 1);

  //TODO: This is not in the app note
  while (this->busy_pin_->digital_read() != true) {
    delay(10);
    App.feed_wdt();
  }
  // set all pins to low
  this->dc_pin_->digital_write(false);
  this->cs_->digital_write(false);
  this->reset_pin_->digital_write(false);
  this->enable_pin_->digital_write(true); //turn off power
}

void PervasiveDisplaysEPaper::send_index_data_(uint8_t index, const uint8_t *data, uint32_t len) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->transfer_byte(index);
  //this->disable(); //TODO: Not required according to app note
  this->dc_pin_->digital_write(true);
  //this->enable();
  for (size_t i = 0; i < len; i++) {
    this->transfer_byte(data[i]);
  }
  
  this->disable();
}

void PervasiveDisplaysEPaper::dump_config() {
  LOG_DISPLAY("", "Pervasive Displays E-Paper", this);
  this->dump_config_model_();
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Temperature: %.1f", this->temperature_);
  LOG_UPDATE_INTERVAL(this);
}


void PervasiveDisplaysEPaper581In::dump_config_model_() {
  ESP_LOGCONFIG(TAG, "  Model: 5.81in");
}

void PervasiveDisplaysEPaper581In::send_duw_drfw_() {
  uint8_t duw[] = {0x00, 0x1f, 0x50, 0x00, 0x1f, 0x03}; //DUW 0x15-0x1A of OTP
  this->send_index_data_(0x13, duw, 6);
  uint8_t drfw[] = {0x00, 0x1f, 0x00, 0xc9}; //DRFW 0x0C-0x0F of OTP
  this->send_index_data_(0x90, drfw, 4);
}

void PervasiveDisplaysEPaper581In::send_ram_rw_() {
  uint8_t raw_rw[] = {0x1f, 0x50, 0x14}; //RAM_RW 0x12-0x14 of OTP
  this->send_index_data_(0x12, raw_rw, 3);
}

int PervasiveDisplaysEPaper581In::get_width_internal() { return 256; }

int PervasiveDisplaysEPaper581In::get_height_internal() { return 720; }


void PervasiveDisplaysEPaper741In::dump_config_model_() {
  ESP_LOGCONFIG(TAG, "  Model: 7.41in");
}

void PervasiveDisplaysEPaper741In::send_duw_drfw_() {
  uint8_t duw[] = {0x00, 0x3b, 0x00, 0x00, 0x1f, 0x03}; //DUW 0x15-0x1A of OTP
  this->send_index_data_(0x13, duw, 6);
  uint8_t drfw[] = {0x00, 0x3b, 0x00, 0xc9}; //DRFW 0x0C-0x0F of OTP
  this->send_index_data_(0x90, drfw, 4);
}

void PervasiveDisplaysEPaper741In::send_ram_rw_() {
  uint8_t ram_rw[] = {0x3b, 0x00, 0x14}; //RAM_RW 0x12-0x14 of OTP
  this->send_index_data_(0x12, ram_rw, 3);
}

int PervasiveDisplaysEPaper741In::get_width_internal() { return 480; }

int PervasiveDisplaysEPaper741In::get_height_internal() { return 800; }

}  // namespace pervasive_displays_epaper
}  // namespace esphome