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

void PervasiveDisplaysEPaper::display() {
  this->enable_pin_->digital_write(false); //turn on power

  this->reset_(200, 20, 200, 50, 5);

  //TODO: There are a lot of hardcoded values here. According to the app note these should be read from OTP
	uint8_t dtcl = 0x08; // 0=IST7232, 8=IST7236
	this->send_index_data_(0x01, &dtcl, 1); //DCTL 0x10 of MTP

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
		delay(100);
    App.feed_wdt();
	}
  uint8_t data18[] = {0x3c};
	this->send_index_data_(0x15, data18, 1); //Display Refresh
	delay(5);

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

void PervasiveDisplaysEPaper::reset_(uint32_t ms1, uint32_t ms2, uint32_t ms3, uint32_t ms4, uint32_t ms5) {
  // note: group delays into one array
	delay(ms1);
  this->reset_pin_->digital_write(true); // RES# = 1
  delay(ms2);
  this->reset_pin_->digital_write(false);
  delay(ms3);
  this->reset_pin_->digital_write(true);
  delay(ms4);
  this->cs_->digital_write(true); // CS# = 1
  delay(ms5);
}

//void PervasiveDisplaysEPaper::DCDC_power_on_() {
//  this->send_index_data_( 0x04, &register_data[0], 1 );  //Power on
//	while( digitalRead( spi_basic.panelBusy ) != HIGH );
//}

void PervasiveDisplaysEPaper::DCDC_soft_start_mid_() {
  // COG init
  uint8_t data4[] = {0x7d};
  this->send_index_data_(0x05, data4, 1);
  delay(200);
  uint8_t data5[] = {0x00};
  this->send_index_data_(0x05, data5, 1);
  delay(10);
  uint8_t data6[] = {0x3f};
  this->send_index_data_(0xc2, data6, 1);
  delay(1);
  uint8_t data7[] = {0x00};
  this->send_index_data_(0xd8, data7, 1); // MS_SYNC mtp_0x1d
  uint8_t data8[] = {0x00};
  this->send_index_data_(0xd6, data8, 1); // BVSS mtp_0x1e
  uint8_t data9[] = {0x10};
  this->send_index_data_(0xa7, data9 , 1);
  delay(100);
  this->send_index_data_(0xa7, data5, 1);
  delay(100);
  // uint8_t data10[] = {0x00, 0x02 };

  uint8_t data10[] = {0x00, 0x01}; // OSC
  this->send_index_data_(0x03, data10, 2); // OSC mtp_0x12

  this->send_index_data_(0x44, data5, 1);
  uint8_t data11[] = {0x80};
  this->send_index_data_(0x45, data11, 1);
  this->send_index_data_(0xa7, data9, 1);
  delay(100);
  this->send_index_data_(0xa7, data7, 1);
  delay(100);
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
  delay(100);
  this->send_index_data_(0xa7, data7, 1);
  delay(100);
  uint8_t data14[] = {0x25};
  this->send_index_data_(0x60, data14, 1); // TCON mtp_0x0b
  // uint8_t data15[] = {0x01 };

  uint8_t data15[] = {0x00}; // STV_DIR
  this->send_index_data_(0x61, data15, 1); // STV_DIR mtp_0x1c

  uint8_t data16[] = {0x00};
  this->send_index_data_(0x01, data16, 1); // DCTL mtp_0x10
  uint8_t data17[] = {0x00};
  this->send_index_data_(0x02, data17, 1); // VCOM mtp_0x11

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
    delay(100);
    App.feed_wdt();
  }
  uint8_t data19[] = {0x7f};
  this->send_index_data_(0x09, data19, 1);
  uint8_t data20[] = {0x7d};
  this->send_index_data_(0x05, data20, 1);
  uint8_t data55[] = {0x00};
  this->send_index_data_(0x09, data55, 1);
  delay(200);

  while (this->busy_pin_->digital_read() != true) {
    delay(100);
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
  this->disable(); //TODO: Not required according to app note
  this->dc_pin_->digital_write(true);
  this->enable();
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
  uint8_t data1[] = {0x00, 0x1f, 0x50, 0x00, 0x1f, 0x03}; // DUW
  this->send_index_data_(0x13, data1, 6); // DUW
  uint8_t data2[] = {0x00, 0x1f, 0x00, 0xc9}; // DRFW
  this->send_index_data_(0x90, data2, 4); // DRFW
}

void PervasiveDisplaysEPaper581In::send_ram_rw_() {
  uint8_t data33[] = {0x1f, 0x50, 0x14}; // RAM_RW
  this->send_index_data_(0x12, data33, 3); // RAM_RW
}

int PervasiveDisplaysEPaper581In::get_width_internal() { return 256; }

int PervasiveDisplaysEPaper581In::get_height_internal() { return 720; }


void PervasiveDisplaysEPaper741In::dump_config_model_() {
  ESP_LOGCONFIG(TAG, "  Model: 7.41in");
}

void PervasiveDisplaysEPaper741In::send_duw_drfw_() {
  uint8_t data1[] = {0x00, 0x3b, 0x00, 0x00, 0x1f, 0x03}; // DUW
  this->send_index_data_(0x13, data1, 6); // DUW
  uint8_t data2[] = {0x00, 0x3b, 0x00, 0xc9}; // DRFW
  this->send_index_data_(0x90, data2, 4); // DRFW
}

void PervasiveDisplaysEPaper741In::send_ram_rw_() {
  uint8_t data34[] = {0x3b, 0x00, 0x14}; // RAM_RW
  this->send_index_data_(0x12, data34, 3); // RAM_RW
}

int PervasiveDisplaysEPaper741In::get_width_internal() { return 480; }

int PervasiveDisplaysEPaper741In::get_height_internal() { return 800; }

}  // namespace pervasive_displays_epaper
}  // namespace esphome