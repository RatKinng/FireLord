#pragma once

#include <Arduino.h>
#include <Wire.h>

// Simple, safe I2C helpers for Arduino Wire (7-bit addresses)
namespace I2C {

  // Initialize I2C; optionally set clock.
  inline void begin(uint32_t clockHz = 400000UL) {
    Wire.begin();            // On SAMD21 XIAO this binds to SDA/SCL pins
    Wire.setClock(clockHz);  // 400 kHz default; change if your parts need 100 kHz
  }

  // Write a single command byte (no register address).
  inline bool writeCmd(uint8_t addr7, uint8_t cmd) {
    Wire.beginTransmission(addr7);
    Wire.write(cmd);
    return (Wire.endTransmission() == 0);
  }

  // Write a single byte to a device register.
  inline bool writeByte(uint8_t addr7, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr7);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
  }

  // Burst-write arbitrary data (e.g., reg + payload)
  inline bool writeBytes(uint8_t addr7, const uint8_t* data, size_t len) {
    Wire.beginTransmission(addr7);
    size_t n = Wire.write(data, len);
    if (n != len) { (void)Wire.endTransmission(); return false; }
    return (Wire.endTransmission() == 0);
  }

  // Read N bytes starting at a register (common “register pointer then read” pattern).
  template <size_t N>
  inline bool readFromReg(uint8_t addr7, uint8_t reg, uint8_t (&out)[N], size_t count) {
    if (count > N) return false;

    // Write register pointer
    Wire.beginTransmission(addr7);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) { // repeated START
      return false;
    }

    // Request bytes
    size_t got = Wire.requestFrom((int)addr7, (int)count, (int)true /*stop*/);
    if (got != count) return false;

    for (size_t i = 0; i < count; ++i) {
      int b = Wire.read();
      if (b < 0) return false;
      out[i] = (uint8_t)b;
    }
    return true;
  }

  // Raw burst-read N bytes (no register write; device must auto-increment or be pre-pointed).
  template <size_t N>
  inline bool readRaw(uint8_t addr7, uint8_t (&out)[N], size_t count) {
    if (count > N) return false;
    size_t got = Wire.requestFrom((int)addr7, (int)count, (int)true /*stop*/);
    if (got != count) return false;

    for (size_t i = 0; i < count; ++i) {
      int b = Wire.read();
      if (b < 0) return false;
      out[i] = (uint8_t)b;
    }
    return true;
  }

  // Quick bus scan (prints to a Stream; defaults to Serial). Returns #devices found.
  inline int scan(Stream &out = Serial) {
    int found = 0;
    out.println(F("I2C scan:"));
    for (uint8_t addr = 1; addr < 127; ++addr) {
      Wire.beginTransmission(addr);
      uint8_t err = Wire.endTransmission();
      if (err == 0) {
        out.print(F("  0x")); if (addr < 16) out.print('0'); out.println(addr, HEX);
        ++found;
      }
    }
    if (found == 0) out.println(F("  (no devices)"));
    return found;
  }

} // namespace I2C
