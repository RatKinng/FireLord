#include <Arduino.h>
#include "I2C_COMMS.h"
#include "UART_COMMS.h"
#include "SamplePacket.h"

constexpr bool LORA_DEBUG      = false;

// ----- Configuration -------------------------------------------------------

constexpr uint8_t DEVICE_ID            = 1;
constexpr uint32_t SAMPLE_INTERVAL_MS  = 30000;   // 30 seconds between uplinks
constexpr uint32_t LORA_FREQUENCY_HZ   = 915000000UL;
constexpr int      LORA_SPREADING_FACT = 7;
constexpr int      LORA_BW_KHZ         = 125;
constexpr int      LORA_PREAMBLE_LEN   = 12;
constexpr int      LORA_TX_POWER_DBM   = 10;

constexpr uint8_t VOC_PIN              = A0;       // D0
constexpr uint8_t SMOKE_PIN            = A1;       // D1

// ----- Sensor state --------------------------------------------------------

namespace {

// Sensirion SCD40 (CO₂, temperature, humidity)
constexpr uint8_t SCD40_ADDR = 0x62;
bool scd40Active = false;

// NXP MPL115A2 (pressure)
constexpr uint8_t MPL115A2_ADDR = 0x60;
bool mpl115a2Active = false;
float mplA0 = 0.0f;
float mplB1 = 0.0f;
float mplB2 = 0.0f;
float mplC12 = 0.0f;

UART_COMMS radio(Serial1, &Serial1);
bool loraReady = false;

} // namespace

// Track recently seen packet CRCs to avoid rebroadcast loops
static uint32_t recentCrcs[8] = {0};
static size_t recentCrcIndex = 0;

static bool hasSeenCrc(uint32_t crc) {
  for (uint32_t stored : recentCrcs) {
    if (stored == crc) return true;
  }
  return false;
}

static void rememberCrc(uint32_t crc) {
  recentCrcs[recentCrcIndex] = crc;
  recentCrcIndex = (recentCrcIndex + 1) % (sizeof(recentCrcs) / sizeof(recentCrcs[0]));
}

// ----- Utility -------------------------------------------------------------

static uint32_t crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; ++i) {
    uint8_t byte = data[i];
    crc ^= byte;
    for (int k = 0; k < 8; ++k) {
      uint32_t mask = -(crc & 1);
      crc = (crc >> 1) ^ (0xEDB88320 & mask);
    }
  }
  return ~crc;
}

static void encodeU16BE(uint8_t *dst, uint16_t value) {
  dst[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
  dst[1] = static_cast<uint8_t>(value & 0xFF);
}

static void encodeS16BE(uint8_t *dst, int16_t value) {
  encodeU16BE(dst, static_cast<uint16_t>(value));
}

static void encodeU32BE(uint8_t *dst, uint32_t value) {
  dst[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
  dst[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
  dst[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
  dst[3] = static_cast<uint8_t>(value & 0xFF);
}

static String toHex(const uint8_t *buf, size_t len) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out += hex[(buf[i] >> 4) & 0x0F];
    out += hex[buf[i] & 0x0F];
  }
  return out;
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  return -1;
}

static bool decodeHex(const String &hex, uint8_t *out, size_t maxLen, size_t &decodedLen) {
  size_t len = hex.length();
  if (len % 2 != 0) return false;
  size_t count = len / 2;
  if (count > maxLen) return false;
  for (size_t i = 0; i < count; ++i) {
    int hi = hexNibble(hex[i * 2]);
    int lo = hexNibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  decodedLen = count;
  return true;
}

static bool sendATDebug(const String &cmd, const char *expect, uint32_t timeout_ms) {
  radio.flushInput();
  if (LORA_DEBUG) {
    Serial.print(F("[LoRa] -> "));
    Serial.println(cmd);
  }
  Serial1.print(cmd);
  if (!cmd.endsWith("\r\n")) {
    Serial1.print("\r\n");
  }

  String resp;
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    while (Serial1.available()) {
      char c = (char)Serial1.read();
      resp += c;
    }
    if (resp.indexOf(expect) >= 0) {
      if (LORA_DEBUG) {
        Serial.print(F("[LoRa] <- "));
        Serial.println(resp);
      }
      return true;
    }
  }
  Serial.print(F("[LoRa] timeout waiting for "));
  Serial.print(expect);
  Serial.print(F(", got: "));
  Serial.println(resp);
  return false;
}

// ----- SCD40 helpers -------------------------------------------------------

static bool scd40StartPeriodicMeasurement() {
  const uint8_t cmd[] = {0x21, 0xB1};
  return I2C::writeBytes(SCD40_ADDR, cmd, sizeof(cmd));
}

static uint8_t scd40CRC(const uint8_t *bytes) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < 2; ++i) {
    crc ^= bytes[i];
    for (int b = 0; b < 8; ++b) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

static bool scd40Read(float &temperatureC, float &humidityPct, uint16_t &co2ppm) {
  if (!scd40Active) return false;

  const uint8_t cmd[] = {0xEC, 0x05};
  if (!I2C::writeBytes(SCD40_ADDR, cmd, sizeof(cmd))) {
    return false;
  }
  delay(5);

  uint8_t raw[9] = {0};
  if (!I2C::readRaw(SCD40_ADDR, raw, sizeof(raw))) {
    return false;
  }

  for (int i = 0; i < 3; ++i) {
    const uint8_t *chunk = &raw[i * 3];
    uint8_t crc = scd40CRC(chunk);
    if (crc != chunk[2]) {
      return false;
    }
  }

  uint16_t co2 = (static_cast<uint16_t>(raw[0]) << 8) | raw[1];
  uint16_t tempRaw = (static_cast<uint16_t>(raw[3]) << 8) | raw[4];
  uint16_t humRaw = (static_cast<uint16_t>(raw[6]) << 8) | raw[7];

  co2ppm = co2;
  temperatureC = -45.0f + 175.0f * (static_cast<float>(tempRaw) / 65535.0f);
  humidityPct = 100.0f * (static_cast<float>(humRaw) / 65535.0f);
  return true;
}

// ----- MPL115A2 helpers ----------------------------------------------------

static bool mpl115a2ReadCoefficients() {
  uint8_t buf[8] = {};
  if (!I2C::readFromReg(MPL115A2_ADDR, 0x04, buf, sizeof(buf))) {
    return false;
  }

  int16_t a0 = (static_cast<int16_t>(buf[0]) << 8) | buf[1];
  int16_t b1 = (static_cast<int16_t>(buf[2]) << 8) | buf[3];
  int16_t b2 = (static_cast<int16_t>(buf[4]) << 8) | buf[5];
  int16_t c12 = ((static_cast<int16_t>(buf[6]) << 8) | buf[7]) >> 2;

  mplA0  = static_cast<float>(a0) / 8.0f;
  mplB1  = static_cast<float>(b1) / 8192.0f;
  mplB2  = static_cast<float>(b2) / 16384.0f;
  mplC12 = static_cast<float>(c12) / 4194304.0f;
  return true;
}

static bool mpl115a2Read(float &pressureKPa) {
  if (!mpl115a2Active) return false;

  const uint8_t startConversion[] = {0x12, 0x00};
  if (!I2C::writeBytes(MPL115A2_ADDR, startConversion, sizeof(startConversion))) {
    return false;
  }
  delay(5);

  uint8_t data[4] = {};
  if (!I2C::readFromReg(MPL115A2_ADDR, 0x00, data, sizeof(data))) {
    return false;
  }

  uint16_t padc = ((static_cast<uint16_t>(data[0]) << 8) | data[1]) >> 6;
  uint16_t tadc = ((static_cast<uint16_t>(data[2]) << 8) | data[3]) >> 6;

  float pComp = mplA0 + (mplB1 + mplC12 * tadc) * padc + mplB2 * tadc;
  pressureKPa = pComp * 0.004f + 0.095f;
  return true;
}

// ----- Sensor orchestration ------------------------------------------------

static void sensorsBegin() {
  scd40Active = scd40StartPeriodicMeasurement();
  mpl115a2Active = mpl115a2ReadCoefficients();
}

static Sample collectSample(uint32_t nowMs) {
  Sample sample{};
  sample.timestamp = nowMs / 1000U;

  float temperatureC = 0.0f;
  float humidityPct = 0.0f;
  uint16_t co2ppm = 0;
  bool ambientOk = scd40Read(temperatureC, humidityPct, co2ppm);

  if (!ambientOk) {
    temperatureC = 21.5f;
    humidityPct = 45.0f;
    co2ppm = 415;
  } else {
    sample.statusFlags |= 0x01;
  }

  float pressureKPa = 0.0f;
  bool pressureOk = mpl115a2Read(pressureKPa);
  if (!pressureOk) {
    pressureKPa = 101.3f;
  } else {
    sample.statusFlags |= 0x02;
  }

  sample.temperatureCx100 = static_cast<int16_t>(temperatureC * 100.0f);
  sample.humidityPctx100  = static_cast<uint16_t>(humidityPct * 100.0f);
  sample.co2ppm           = co2ppm;
  sample.pressureDeciHpa  = static_cast<uint16_t>(pressureKPa * 100.0f); // kPa -> hPa*10

  sample.vocRaw   = analogRead(VOC_PIN);
  sample.smokeRaw = analogRead(SMOKE_PIN);

  if (sample.vocRaw >= 4090 || sample.smokeRaw >= 4090) {
    sample.statusFlags |= 0x04;
  }

  return sample;
}

// ----- LoRa helpers --------------------------------------------------------

static bool configureLoRa() {
  if (LORA_DEBUG) Serial.println(F("[LoRa] configuring radio..."));
  if (!sendATDebug(String(F("AT")), "+AT: OK", 1000)) {
    Serial.println(F("[LoRa] unable to reach module"));
    return false;
  }
  delay(200);
  if (!sendATDebug(String(F("AT+MODE=TEST")), "+MODE: TEST", 1000)) {
    Serial.println(F("[LoRa] failed to enter test mode"));
    return false;
  }
  delay(200);
  int freqMHz = static_cast<int>(LORA_FREQUENCY_HZ / 1000000UL);
  String cfg = String("AT+TEST=RFCFG,") + String(freqMHz) + ",SF" + String(LORA_SPREADING_FACT) + "," + String(LORA_BW_KHZ) + "," + String(LORA_PREAMBLE_LEN) + ",15," + String(LORA_TX_POWER_DBM) + ",ON,OFF,OFF";
  if (!sendATDebug(cfg, "+TEST: RFCFG", 1000)) {
    Serial.println(F("[LoRa] RFCFG command failed"));
    return false;
  }
  delay(200);
  sendATDebug(String(F("AT+TEST=RXLRPKT")), "+TEST: RXLRPKT", 500);
  if (LORA_DEBUG) Serial.println(F("[LoRa] configuration OK"));
  return true;
}

static uint32_t buildPacket(const Sample &sample, uint8_t *out, size_t &outLen) {
  constexpr uint8_t VERSION = 1;

  out[0] = VERSION;
  out[1] = DEVICE_ID;
  encodeU32BE(&out[2], sample.timestamp);
  encodeS16BE(&out[6], sample.temperatureCx100);
  encodeU16BE(&out[8], sample.humidityPctx100);
  encodeU16BE(&out[10], sample.co2ppm);
  encodeU16BE(&out[12], sample.pressureDeciHpa);
  encodeU16BE(&out[14], sample.vocRaw);
  encodeU16BE(&out[16], sample.smokeRaw);
  out[18] = sample.statusFlags;

  const size_t payloadLen = 19;
  uint32_t crc = crc32(out, payloadLen);
  encodeU32BE(&out[19], crc);
  outLen = payloadLen + 4;
  return crc;
}

static void logSample(const Sample &sample) {
  Serial.print(F("t="));
  Serial.print(sample.timestamp);
  Serial.print(F("s, temp="));
  Serial.print(sample.temperatureCx100 / 100.0f);
  Serial.print(F("C, hum="));
  Serial.print(sample.humidityPctx100 / 100.0f);
  Serial.print(F("%, co2="));
  Serial.print(sample.co2ppm);
  Serial.print(F("ppm, pressure="));
  Serial.print(sample.pressureDeciHpa / 10.0f);
  Serial.print(F(" hPa, voc="));
  Serial.print(sample.vocRaw);
  Serial.print(F(", smoke="));
  Serial.print(sample.smokeRaw);
  Serial.print(F(", flags=0x"));
  Serial.println(sample.statusFlags, HEX);
}

static void pollLoRaReceive() {
  if (!loraReady) return;

  for (;;) {
    String line;
    if (!radio.readLine(line, 200)) break;
    line.trim();
    if (line.length() == 0) continue;
    bool isRxLine = line.startsWith("+TEST: RXLRPKT") || line.startsWith("+TEST: RX \"");
    if (!isRxLine) {
      Serial.print(F("[LoRa] unsolicited: "));
      Serial.println(line);
      continue;
    }

    int quoteStart = line.indexOf('"');
    int quoteEnd = line.lastIndexOf('"');
    String hexPayload;
    if (quoteStart >= 0 && quoteEnd > quoteStart) {
      hexPayload = line.substring(quoteStart + 1, quoteEnd);
    } else {
      int comma = line.lastIndexOf(',');
      if (comma >= 0 && comma + 1 < line.length()) {
        hexPayload = line.substring(comma + 1);
      } else {
        continue;
      }
    }
    hexPayload.trim();

    uint8_t buf[32] = {};
    size_t decoded = 0;
    if (!decodeHex(hexPayload, buf, sizeof(buf), decoded)) {
      Serial.println(F("[RX] unable to decode payload hex"));
      continue;
    }
    if (decoded < 23) {
      Serial.print(F("[RX] payload too short: "));
      Serial.println(decoded);
      continue;
    }

    uint32_t crcRx = (static_cast<uint32_t>(buf[19]) << 24) |
                     (static_cast<uint32_t>(buf[20]) << 16) |
                     (static_cast<uint32_t>(buf[21]) << 8) |
                     static_cast<uint32_t>(buf[22]);
    if (crcRx != crc32(buf, 19)) {
      Serial.println(F("[RX] CRC mismatch"));
      continue;
    }
    if (hasSeenCrc(crcRx)) {
      if (LORA_DEBUG) Serial.println(F("[RX] duplicate packet, skipping"));
      continue;
    }
    rememberCrc(crcRx);

    uint8_t version = buf[0];
    uint8_t nodeId = buf[1];
    uint32_t ts = (static_cast<uint32_t>(buf[2]) << 24) |
                  (static_cast<uint32_t>(buf[3]) << 16) |
                  (static_cast<uint32_t>(buf[4]) << 8) |
                  static_cast<uint32_t>(buf[5]);
    uint16_t tempCx100 = (static_cast<uint16_t>(buf[6]) << 8) | buf[7];
    uint16_t humidity = (static_cast<uint16_t>(buf[8]) << 8) | buf[9];
    uint16_t co2 = (static_cast<uint16_t>(buf[10]) << 8) | buf[11];
    uint16_t pressure = (static_cast<uint16_t>(buf[12]) << 8) | buf[13];
    uint16_t voc = (static_cast<uint16_t>(buf[14]) << 8) | buf[15];
    uint16_t smoke = (static_cast<uint16_t>(buf[16]) << 8) | buf[17];
    uint8_t flags = buf[18];

    Serial.print(F("[RX] node="));
    Serial.print(nodeId);
    Serial.print(F(", ver="));
    Serial.print(version);
    Serial.print(F(", ts="));
    Serial.print(ts);
    Serial.print(F("s, temp="));
    Serial.print(static_cast<int16_t>(tempCx100) / 100.0f);
    Serial.print(F("C, hum="));
    Serial.print(humidity / 100.0f);
    Serial.print(F("%, co2="));
    Serial.print(co2);
    Serial.print(F("ppm, pressure="));
    Serial.print(pressure / 10.0f);
    Serial.print(F(" hPa, voc="));
    Serial.print(voc);
    Serial.print(F(", smoke="));
    Serial.print(smoke);
    Serial.print(F(", flags=0x"));
    Serial.println(flags, HEX);

    if (nodeId != DEVICE_ID) {
      Serial.print(F("[FWD] payload="));
    Serial.println(hexPayload);
    if (!LoRaE5::p2pTxHex(radio, hexPayload)) {
        Serial.println(F("[WARN] Forward TX failed"));
      }
    }

    sendATDebug(String(F("AT+TEST=RXLRPKT")), "+TEST: RXLRPKT", 500);
  }
}

// ----- Arduino entry points ------------------------------------------------

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  analogReadResolution(12);
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    ; // wait for host
  }

  pinMode(VOC_PIN, INPUT);
  pinMode(SMOKE_PIN, INPUT);
  I2C::begin();
  sensorsBegin();

  Serial1.begin(9600);
  loraReady = configureLoRa();
  if (!loraReady) {
    Serial.println(F("[WARN] LoRa configuration failed; will keep retrying on send."));
  } else {
    Serial.println(F("[INFO] LoRa ready."));
  }
}

void loop() {
  pollLoRaReceive();

  static uint32_t lastSendMs = 0;
  const uint32_t now = millis();
  if (now - lastSendMs < SAMPLE_INTERVAL_MS) {
    delay(5);
    return;
  }
  lastSendMs = now;

  Sample sample = collectSample(now);
  logSample(sample);

  uint8_t packet[23] = {};
  size_t packetLen = 0;
  uint32_t crc = buildPacket(sample, packet, packetLen);
  rememberCrc(crc);

  if (!loraReady) {
    loraReady = configureLoRa();
  }

  if (loraReady) {
    String hex = toHex(packet, packetLen);
    if (LORA_DEBUG) {
      Serial.print(F("[TX] payload="));
      Serial.println(hex);
    }
    if (!LoRaE5::p2pTxHex(radio, hex)) {
      Serial.println(F("[WARN] LoRa TX failed; will retry next interval."));
      loraReady = false;
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
    }
    sendATDebug(String(F("AT+TEST=RXLRPKT")), "+TEST: RXLRPKT", 500);
  }
}
