#include "external_dac.h"

#ifdef I2S_EXTERNAL_DAC

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace i2s_audio {

static const char *const TAG = "i2s_external_dac";

bool AW88298::init_device(){
  if( this->enable_pin_ != nullptr ){
    this->enable_pin_->digital_write(true);
  }

  uint16_t val = 0x0673;
  this->write_bytes_16(0x61, &val, 1 ); // AW88298_REG_BSTCTRL2

  // enabled I2EN and switch to operation mode
  val = 0x4042; // I2SEN=1 AMPPD=1 PWDN=0
  this->write_bytes_16(0x04, &val, 1); // AW88298_REG_SYSCTRL

  val = 0x0008; // RMSE=0 HAGCE=0 HDCCE=0 HMUTE=0
  this->write_bytes_16(0x05, &val, 1); //AW88298_REG_SYSCTRL2
  return true;
}

bool AW88298::apply_i2s_settings(const i2s_driver_config_t&  i2s_cfg){
  esph_log_d(TAG, "Setup AW88298");

  uint16_t val  = 0x4000; // I2SEN=1 AMPPD=0 PWDN=0
             val |= (1 << 6) ; // I2S Enable
             val |= (1 << 1) ; // power down amp for configuration
  this->write_bytes_16(0x04,  &val, 1);

  //sampling rate
  static constexpr uint8_t rate_tbl[] = {4,5,6,8,10,11,15,20,22,44};
  uint8_t rate_idx = 0;
  size_t rate = ( i2s_cfg.sample_rate + 1102) / 2205;
  while (rate > rate_tbl[rate_idx] && ++rate_idx < sizeof(rate_tbl)) {}

  //bits_per_sample
  static constexpr uint8_t supported_bps[] = {16,20,24,32};
  uint8_t bps = (uint8_t) i2s_cfg.bits_per_sample;
  uint8_t bps_idx = 0;
  while( bps != supported_bps[bps_idx] && ++bps_idx < sizeof(supported_bps)) {}
  if( bps_idx == sizeof(supported_bps))
  {
    esph_log_e(TAG, "Unsupported bits per sample: %d", bps);
    return false;
  }

  //bits_per_frame
  //00: 16*2 01: 24*2 10: 32*2 11:reserved
  uint8_t i2sbck = 0;

  size_t reg0x06_value = (1 << 12); //enable I2S receiver module
  reg0x06_value |=       (3 << 10); // mono, (L+R)/2
  reg0x06_value |= bps_idx  <<  6;
  reg0x06_value |= i2sbck   <<  4;
  reg0x06_value |= rate_idx;

  //reg0x06_value |= 0x1C00;  // I2SBCK=0 (BCK mode 16*2)
  this->write_bytes_16(0x06, (uint16_t *) &reg0x06_value, 1 );

  val  = 0x4000; // I2SEN=1 AMPPD=0 PWDN=0
             val |= (1 << 6) ; // I2S Enable
             val |= (0 << 1) ; // power up amp for configuration
  this->write_bytes_16(0x04,  &val, 1);
  return true;
}

bool AW88298::set_volume( float volume ){
  // 0 to 96 dB
  // 7:4 in unit of -6dB
  // 3:0 in unit of -0.5dB
  uint16_t val = (1. - volume) * 192.;
  val = (val / 12) << 4 | (val % 12);
  val = (val << 8 ) | 0x0064;
  this->write_bytes_16(0x0C, &val, 1 ); // AW88298_REG_HAGCCFG3
  return true;
}

bool AW88298::set_mute_audio( bool mute ){
  // Hardware mute module SYSCTRL2 (0x05) register bit 4
  uint16_t val;
  this->read_bytes_16(0x05, &val, 1);
  if( mute ){
    val |= (1 << 4);
  }
  else {
    val &= ~(1 << 4);
  }
  this->write_bytes_16(0x05, &val, 1);
  return true;
}

bool ES8388::init_device(){
  // mute
  this->write_byte(0x19, 0x04);
  // powerup
  this->write_byte(0x01, 0x50);
  this->write_byte(0x02, 0x00);
  // worker mode
  this->write_byte(0x08, 0x00);
  // DAC powerdown
  this->write_byte(0x04, 0xC0);
  // vmidsel/500k ADC/DAC idem
  this->write_byte(0x00, 0x12);

  // i2s 16 bits
  this->write_byte(0x17, 0x18);
  // sample freq 256
  this->write_byte(0x18, 0x02);
  // LIN2/RIN2 for mixer
  this->write_byte(0x26, 0x00);
  // left DAC to left mixer
  this->write_byte(0x27, 0x90);
  // right DAC to right mixer
  this->write_byte(0x2A, 0x90);
  // DACLRC ADCLRC idem
  this->write_byte(0x2B, 0x80);
  this->write_byte(0x2D, 0x00);
  // DAC volume max
  this->write_byte(0x1B, 0x00);
  this->write_byte(0x1A, 0x00);

  // ADC poweroff
  this->write_byte(0x03, 0xFF);
  // ADC amp 24dB
  this->write_byte(0x09, 0x88);
  // LINPUT1/RINPUT1
  this->write_byte(0x0A, 0x00);
  // ADC mono left
  this->write_byte(0x0B, 0x02);
  // i2S 16b
  this->write_byte(0x0C, 0x0C);
  // MCLK 256
  this->write_byte(0x0D, 0x02);
  // ADC Volume
  this->write_byte(0x10, 0x00);
  this->write_byte(0x11, 0x00);
  // ALC OFF
  this->write_byte(0x03, 0x09);
  this->write_byte(0x2B, 0x80);

  this->write_byte(0x02, 0xF0);
  delay(1);
  this->write_byte(0x02, 0x00);
  // DAC power-up LOUT1/ROUT1 enabled
  this->write_byte(0x04, 0x30);
  this->write_byte(0x03, 0x00);
  // DAC volume max
  this->write_byte(0x2E, 0x1C);
  this->write_byte(0x2F, 0x1C);
  // unmute
  this->write_byte(0x19, 0x00);
  return true;
}


bool ES8388::apply_i2s_settings(const i2s_driver_config_t&  i2s_cfg){
  // i2s 16 bits
  //this->write_byte(0x17, 0x18);
  return true;
}

bool ES8388::set_mute_audio( bool mute ){
  // mute / unmute
  this->write_byte(0x19, mute ? 0x04 : 0x00 );
  return true;
}

#define AC101_ERROR_CHECK(func)                                                \
  if (!(func)) {                                                               \
    esph_log_d(TAG, "AC101 failed");                                           \
    return false;                                                               \
  }
#define AC101_READ_REG(reg, value) AC101_ERROR_CHECK(this->ReadReg(reg, value));
#define AC101_WRITE_REG(reg, value)                                            \
  AC101_ERROR_CHECK(this->WriteReg(reg, value));

bool AC101::WriteReg(uint8_t reg, uint16_t value) {
  return this->write_bytes_16(reg, &value, 1);
}

bool AC101::ReadReg(uint8_t reg, uint16_t *value) {
  return this->read_bytes_16(reg, value, 1);
}


bool AC101::init_device(){
  uint16_t val;
  // Reset all registers, readback default as sanity check
  AC101_WRITE_REG(AC101_CHIP_AUDIO_RS, 0x0123);
  delay(100);
  {
    AC101_READ_REG(AC101_CHIP_AUDIO_RS, &val);
    if (val != 0x0101) {
      esph_log_d(TAG, "AC101 failed");
      return false;
    }
  }

  AC101_WRITE_REG(AC101_SPKOUT_CTRL, 0xe880);

  // Enable the PLL from 256*44.1KHz MCLK source
  AC101_WRITE_REG(AC101_PLL_CTRL1, 0x014f);
  AC101_WRITE_REG(AC101_PLL_CTRL2, 0x8600);

  // Clocking system
  AC101_WRITE_REG(AC101_SYSCLK_CTRL, 0x8b08);
  AC101_WRITE_REG(AC101_MOD_CLK_ENA, 0x800c);
  AC101_WRITE_REG(AC101_MOD_RST_CTRL, 0x800c);
    // Set default at I2S, 44.1KHz, 16bit
  AC101_WRITE_REG(AC101_I2S_SR_CTRL, 0x7000);
//SetI2sClock
  AC101_READ_REG(AC101_I2S1LCK_CTRL, &val);
  val &= ~0x7FC0;
  val |= uint16_t(0) << 14;
  val |= uint16_t(0x4) << 9;
  val |= uint16_t(0) << 13;
  val |= uint16_t(0x1) << 6;
  //SetI2sMode
  AC101_WRITE_REG(AC101_I2S1LCK_CTRL, val);
  AC101_READ_REG(AC101_I2S1LCK_CTRL, &val);
  val &= ~0x8000;
  val |= uint16_t(0x01) << 15;
  AC101_WRITE_REG(AC101_I2S1LCK_CTRL, val);
  //SetI2sWordSize
  AC101_READ_REG(AC101_I2S1LCK_CTRL, &val);
  val &= ~0x0030;
  val |= uint16_t(0x01) << 4;
  AC101_WRITE_REG(AC101_I2S1LCK_CTRL, val);
  //SetI2sFormat
  AC101_READ_REG(AC101_I2S1LCK_CTRL, &val);
  val &= ~0x000C;
  val |= uint16_t(0x00) << 2;
  AC101_WRITE_REG(AC101_I2S1LCK_CTRL, val);
  // AIF config
  AC101_WRITE_REG(AC101_I2S1_SDOUT_CTRL, 0xc000);
  AC101_WRITE_REG(AC101_I2S1_SDIN_CTRL, 0xc000);
  AC101_WRITE_REG(AC101_I2S1_MXR_SRC, 0x2200);

  AC101_WRITE_REG(AC101_ADC_SRCBST_CTRL, 0xccc4);
  AC101_WRITE_REG(AC101_ADC_SRC, 0x2020);
  AC101_WRITE_REG(AC101_ADC_DIG_CTRL, 0x8000);
  AC101_WRITE_REG(AC101_ADC_APC_CTRL, 0xbbc3);

  // Path Configuration
  AC101_WRITE_REG(AC101_DAC_MXR_SRC, 0xcc00);
  AC101_WRITE_REG(AC101_DAC_DIG_CTRL, 0x8000);
  AC101_WRITE_REG(AC101_OMIXER_SR, 0x0081);
  AC101_WRITE_REG(AC101_OMIXER_DACA_CTRL, 0xf080);

  //SetMode MODE_ADC_DAC
  AC101_WRITE_REG(AC101_MOD_CLK_ENA, 0x800c);
  AC101_WRITE_REG(AC101_MOD_RST_CTRL, 0x800c);
  AC101_WRITE_REG(AC101_OMIXER_DACA_CTRL, 0xff80);
  delay(100);
  AC101_WRITE_REG(AC101_HPOUT_CTRL, 0xfbc0);
  AC101_WRITE_REG(AC101_SPKOUT_CTRL, 0xeabd);
  this->set_volume(63);
  return true;
}


bool AC101::apply_i2s_settings(const i2s_driver_config_t&  i2s_cfg){
  esph_log_d(TAG, "Setup AC101");
  uint16_t rate;
  switch (i2s_cfg.sample_rate) {
        case 8000:
            rate = 0x0000;
            break;
        case 11052:
            rate = 0x1000;
            break;
        case 12000:
            rate = 0x2000;
            break;
        case 16000:
            rate = 0x3000;
            break;
        case 22050:
            rate = 0x4000;
            break;
        case 24000:
            rate = 0x5000;
            break;
        case 32000:
            rate = 0x6000;
            break;
        case 44100:
            rate = 0x7000;
            break;
        case 48000:
            rate = 0x8000;
            break;
        case 96000:
            rate = 0x9000;
            break;
        case 192000:
            rate = 0xa000;
            break;
        default:
            rate = 0x3000;
            esph_log_d(TAG,"Taxa de amostragem inválida");
    }
  AC101_WRITE_REG(AC101_I2S_SR_CTRL, rate);
  //bits_per_sample
    uint8_t bits;

    switch (i2s_cfg.bits_per_sample) {
        case 8:
            bits = 0x00;
            break;
        case 16:
            bits = 0x01;
            break;
        case 20:
            bits = 0x02;
            break;
        case 24:
            bits = 0x03;
            break;
        default:
            rate = 0x01;
            esph_log_e(TAG, "Unsupported bits per sample: %d", bitsPerSample);
    }
  uint16_t val;
  AC101_READ_REG(AC101_I2S1LCK_CTRL, &val);
  val &= ~0x0030;
  val |= uint16_t(bits) << 4;
  AC101_WRITE_REG(AC101_I2S1LCK_CTRL, val);
  
  return true;
}

bool AC101::set_volume( float volume ){
  uint8_t int_volume = volume * 100.;
  if (int_volume > 63)
    int_volume = 63;

  uint16_t val;
  AC101_READ_REG(AC101_HPOUT_CTRL, &val);
  val &= ~(0x3F << 4);  // Corrigido para limpar os bits corretos
  val |= (int_volume & 0x3F) << 4;  
  AC101_WRITE_REG(AC101_HPOUT_CTRL, val); 
  
  int_volume /= 2;
  if (int_volume > 31)
    int_volume = 31;

  AC101_READ_REG(AC101_SPKOUT_CTRL, &val);
  val &= ~31;
  val |= int_volume;
  AC101_WRITE_REG(AC101_SPKOUT_CTRL, val);
  return true;
}
bool AC101::set_mute_audio( bool mute ){
  return true;
}
}
}

#endif
