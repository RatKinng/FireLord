#pragma once
#include <Arduino.h>

// Simple UART helper around any Stream (Serial1 recommended on XIAO SAMD21).
class UART_COMMS {
public:
  explicit UART_COMMS(Stream &serial) : io(serial) {}

  // For HardwareSerial-like objects that support begin()
  void begin(unsigned long baud) {
    // If this Stream also has begin(), call it via downcast
    if (HardwareSerial *hs = dynamic_cast<HardwareSerial*>(&io)) {
      hs->begin(baud);
    }
  }

  // Write raw bytes
  size_t write(const uint8_t *buf, size_t len) { return io.write(buf, len); }
  size_t write(const String &s) { return io.print(s); }
  size_t println(const String &s) { size_t n = io.print(s); n += io.print("\r\n"); return n; }

  // Read into buffer with timeout (ms). Returns bytes read.
  size_t readBytes(uint8_t *buf, size_t len, uint32_t timeout_ms = 1000) {
    io.setTimeout(timeout_ms);
    return io.readBytes(buf, len);
  }

  // Read a line ending in \n (strips CRLF). Returns true on success.
  bool readLine(String &out, uint32_t timeout_ms = 1000) {
    uint32_t start = millis();
    out = "";
    while (millis() - start < timeout_ms) {
      while (io.available()) {
        char c = (char)io.read();
        if (c == '\r') continue;
        if (c == '\n') return true;
        out += c;
      }
      yield();
    }
    return false;
  }

  // Read until token appears or timeout. Captures into out (without token).
  bool readUntil(const String &token, String &out, uint32_t timeout_ms = 1000) {
    uint32_t start = millis();
    out = "";
    while (millis() - start < timeout_ms) {
      while (io.available()) {
        char c = (char)io.read();
        out += c;
        if (out.endsWith(token)) {
          out.remove(out.length() - token.length());
          return true;
        }
      }
      yield();
    }
    return false;
  }

  // Flush any pending input
  void flushInput() {
    while (io.available()) (void)io.read();
  }

  // Send an AT command with optional CRLF. Returns true if any expected token appears.
  bool sendAT(const String &cmd, const String &expect = "OK", uint32_t timeout_ms = 2000, bool addCRLF = true) {
    flushInput();
    if (addCRLF) {
      io.print(cmd); io.print("\r\n");
    } else {
      io.print(cmd);
    }
    String resp;
    return readUntil(expect, resp, timeout_ms);
  }

  // Collect full response until a terminator token (e.g., "OK" or "ERROR")
  bool sendATGetResp(const String &cmd, String &resp, const String &term1 = "OK",
                     const String &term2 = "ERROR", uint32_t timeout_ms = 2000) {
    flushInput();
    io.print(cmd); io.print("\r\n");
    resp = "";
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
      while (io.available()) {
        char c = (char)io.read();
        resp += c;
        if (resp.endsWith(term1)) return true;
        if (resp.endsWith(term2)) return false;
      }
      yield();
    }
    return false; // timed out
  }

  // Hex dump a buffer to the UART (useful for debugging)
  void hexDump(const uint8_t *buf, size_t len) {
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; ++i) {
      io.write(hex[(buf[i] >> 4) & 0xF]);
      io.write(hex[buf[i] & 0xF]);
      if (i + 1 < len) io.write(' ');
    }
    io.print("\r\n");
  }

private:
  Stream &io;
};

// Convenience helpers for Seeed LoRa-E5 AT commands over UART.
namespace LoRaE5 {
  // Basic ping: returns true if module answers OK
  inline bool ping(UART_COMMS &u) { return u.sendAT("AT"); }

  // Get firmware version into out
  inline bool version(UART_COMMS &u, String &out) { return u.sendATGetResp("AT+VER=?", out); }

  // Set LoRaWAN mode: 0=LoRaWAN, 1=FSK, 2=LoRa P2P (check your use case)
  inline bool setMode(UART_COMMS &u, int mode) { return u.sendAT("AT+MODE=" + String(mode)); }

  // Example P2P config: freq Hz, sf, bw kHz, preamble, txpwr dBm
  inline bool p2pConfig(UART_COMMS &u, uint32_t freq, int sf, int bw_khz, int preamble, int pwr) {
    return u.sendAT("AT+TEST=RFCFG," + String(freq) + "," + String(sf) + "," + String(bw_khz) + "," + String(preamble) + "," + String(pwr));
  }

  // Transmit hex payload in P2P test mode
  inline bool p2pTxHex(UART_COMMS &u, const String &hexPayload) {
    return u.sendAT("AT+TEST=TXLRPKT," + hexPayload);
  }
}
