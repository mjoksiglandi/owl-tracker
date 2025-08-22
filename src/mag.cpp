#include "mag.h"
#include "board_pins.h"
#include <Wire.h>

static const uint8_t IST8310_ADDR = 0x0E; // 7-bit usual

static MagData g_mag;
static bool    g_present = false;

static bool i2c_read8(uint8_t addr, uint8_t reg, uint8_t &val){
  Wire.beginTransmission(addr); Wire.write(reg);
  if(Wire.endTransmission(false)!=0) return false;
  if(Wire.requestFrom((int)addr,1)!=1) return false;
  val = Wire.read(); return true;
}

bool mag_begin(){
  // Wire ya iniciado por baro_begin(); si no:
  // Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);

  uint8_t who=0;
  if(!i2c_read8(IST8310_ADDR, 0x00, who)){ g_present=false; }
  else { g_present=true; }
  g_mag = MagData(); g_mag.present=g_present;
  return g_present;
}

void mag_poll(){
  if(!g_present) return;
  // TODO: configurar y leer XYZ del IST8310 (pendiente mapeo exacto del módulo)
}

MagData mag_last(){ return g_mag; }
