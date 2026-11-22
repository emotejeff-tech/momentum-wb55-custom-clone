/*
 * GM12864-59N (ST7567S, I2C) diagnostic sketch.
 *
 * Purpose: prove the panel and your wiring independently of the STM32 and
 * Momentum. Flash this to any Arduino/ESP32 board, open the serial monitor at
 * 115200, and follow the output.
 *
 * Wiring (3.3 V board!):
 *    LCD 3V3 -> 3V3
 *    LCD GND -> GND
 *    LCD SCL -> SDA/SCL of your board (ESP32 default: GPIO22)
 *    LCD SDA -> SDA of your board      (ESP32 default: GPIO21)
 *    plus 4.7k pull-ups to 3V3 if the module has none.
 *
 * This is a diagnostic, not the production driver. The production driver is
 * firmware/display_driver_patch/st7567_i2c.c.
 */

#include <Wire.h>
#include <string.h>

static const uint8_t ADDR_MIN = 0x3C;
static const uint8_t ADDR_MAX = 0x3F;
static uint8_t g_addr = 0;

static void printHex(uint8_t value) {
  Serial.print("0x");
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

static void cmd(uint8_t c) {
  Wire.beginTransmission(g_addr);
  Wire.write(0x00);
  Wire.write(c);
  Wire.endTransmission();
}

static void cmd2(uint8_t c, uint8_t a) {
  Wire.beginTransmission(g_addr);
  Wire.write(0x00);
  Wire.write(c);
  Wire.write(a);
  Wire.endTransmission();
}

static void data(const uint8_t* d, size_t n) {
  Wire.beginTransmission(g_addr);
  Wire.write(0x40);
  for (size_t i = 0; i < n; i++) Wire.write(d[i]);
  Wire.endTransmission();
}

static uint8_t scan() {
  uint8_t found = 0;
  for (uint8_t a = ADDR_MIN; a <= ADDR_MAX; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print("  ACK at "); printHex(a); Serial.println();
      if (!found) found = a;
    }
  }
  return found;
}

static void initPanel() {
  cmd(0xE2); delay(5);            // software reset
  cmd(0xA2);                      // 1/9 bias
  cmd(0xA0);                      // SEG normal
  cmd(0xC8);                      // COM reverse
  cmd(0x40);                      // start line 0
  cmd(0x25);                      // regulation ratio
  cmd2(0x81, 0x28);               // contrast
  cmd2(0xF8, 0x00);               // booster 4x
  cmd(0x2C); delay(2);            // booster on
  cmd(0x2E); delay(2);            // regulator on
  cmd(0x2F); delay(2);            // follower on
  cmd(0xA4);                      // all pixels off
  cmd(0xA6);                      // not inverted
  cmd(0xAF);                      // display on
}

static void fill(uint8_t pattern) {
  uint8_t line[128];
  memset(line, pattern, sizeof(line));
  for (uint8_t page = 0; page < 8; page++) {
    cmd(0xB0 | page);
    cmd(0x10);
    cmd(0x00);
    data(line, sizeof(line));
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin();
  Wire.setClock(100000);  // start slow; 400k only with short wires

  Serial.println("\nGM12864-59N / ST7567S diagnostic");
  Serial.println("Scanning 0x3C..0x3F ...");
  g_addr = scan();
  if (!g_addr) {
    Serial.println("No device. Check 3V3/GND/SDA/SCL, pull-ups, and whether");
    Serial.println("the module is really I2C (4 pins) and not SPI (8 pins).");
    return;
  }

  Serial.print("Using "); printHex(g_addr); Serial.println();
  initPanel();
  Serial.println("Init sent. Expect: white -> black -> checkerboard -> sweep.");

  fill(0xFF); delay(800);
  fill(0x00); delay(800);
  fill(0xAA); delay(1200);

  for (uint8_t contrast = 0x10; contrast <= 0x38; contrast += 8) {
    Serial.print("contrast "); printHex(contrast); Serial.println();
    cmd2(0x81, contrast);
    fill(0xAA);
    delay(700);
  }
  Serial.println("Done. If the pattern is mirrored, flip SEG/COM (A1/C0).");
  Serial.println("If it is faint, try booster 5x (F8 01).");
}

void loop() {
  delay(1000);
}
