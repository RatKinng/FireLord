# Device Firmware Guide

The firmware runs on a Seeeduino XIAO SAMD21 (or comparable 3.3 V Arduino board) and drives the Reyax-class LoRa module plus the sensor suite. This outline assumes Arduino IDE or PlatformIO as the build tool.

---

## Inputs and Dependencies

- MCU: Seeeduino XIAO SAMD21 with hardware UART and I²C.  
- LoRa radio: Grove Wio-E5, Reyax RYLR998, or any AT-driven LoRa-E5 module on 3.3 V logic.  
- Sensors: SCD40 (I²C), MQ-7 (analog), MPL115A2 (I²C), MEMS smoke (analog), MEMS VOC (analog).  
- Host tooling: Arduino IDE or PlatformIO with Seeed SAMD board package.  
- Helpers: `I2C_COMMS`, `UART_COMMS`, and `SamplePacket.h`; optional `pseudo.cpp` shows an early loop.

---

## File Reference

| File | Purpose |
| --- | --- |
| `FireLordNode.ino` | Main sketch for the field node; samples sensors and forwards packets via LoRa-E5. |
| `I2C_COMMS.*` | Safe wrappers for register reads and raw bursts. |
| `UART_COMMS.*` | Stream helper with LoRa-E5 AT shortcuts. |
| `pseudo.cpp` | Example loop covering sampling, validation, and packet assembly. |

Import the sketch folder (`FireLordNode.ino`, helper headers, and sources) into Arduino IDE or PlatformIO as-is; avoid renaming the files because the build references them directly.

---

## Bring-Up Checklist

1. Install the “Seeed SAMD Boards” package in Arduino IDE (or PlatformIO equivalent).  
2. Create a project, add the helper headers, and include them in `setup()`.  
3. Wire sensors to SDA/SCL and analog inputs; connect the LoRa module to `Serial1`.  
4. Run an I²C scan and `LoRaE5::ping()` to confirm hardware connectivity.

---

## Sampling Pattern

Each node wakes every 30 s, samples local sensors, and packages readings into a fixed 23-byte payload plus a CRC32 footer. The CRC is stored in a small ring buffer so the node can recognise its own transmissions and recent relays. The high-level flow is:

1. Wake on a timer (e.g., 60 s).  
2. Read I²C sensors with `I2C::readFromReg`.  
3. Sample analog channels after heater warm-up.  
4. Validate ranges and set flag bits for outliers.  
5. Package values, record the CRC, and hand off for LoRa transmission.

Keep a short hash history to reject duplicate packets and duty-cycle heater loads to save power.

---

## LoRa Configuration

```cpp
UART_COMMS radio(Serial1);
radio.begin(9600);
LoRaE5::setMode(radio, 2); // P2P mode
LoRaE5::p2pConfig(radio, 915000000UL, 7, 125, 12, 10);
```

Configuration mirrors Seeed’s Wio-E5 examples: the firmware issues `AT`, `AT+MODE=TEST`, `AT+TEST=RFCFG,…`, and `AT+TEST=RXLRPKT` during boot. `UART_COMMS::p2pTxHex` waits for a `+TEST: TX DONE` response so the code knows if the send succeeded. A rolling set of CRCs prevents retransmitting the same payload again.

---

## Suggested Packet Layout

```
0   : Protocol version
1   : Device ID
2-7 : UNIX timestamp
8-9 : Temperature (°C ×100)
10-11: Humidity (% ×100)
12-13: CO₂ (ppm)
14-15: CO (ppm ×10)
16-17: Pressure (Pa offset)
18  : Smoke index (0-255)
19  : VOC index (0-255)
20  : Status flags
21-24: Packet hash (CRC32 or similar)
```

Stay below 51 bytes to remain compatible with common LoRa P2P settings.

---

## Power Practices

- Sleep between samples using an RTC alarm or LowPower helper.  
- Switch heater-powered sensors (smoke/VOC) with MOSFETs to keep the ≈116 mW average target.  
- Monitor the supply rail; defer transmissions if the supercapacitor droops.  
- Adjust `SAMPLE_INTERVAL_MS` if you need faster or slower updates.

Reference power budget: ~283 mW receive-ready with heaters on; ~116 mW with the default 5 % heater duty-cycle. Supercapacitors sized at 600 mAh-equivalent give ~7 h of autonomy without sun.

---

## Testing and Maintenance

- Bench test with sensors unplugged; the sketch falls back to seeded data and sets status flags when live sensors are missing.  
- Watch serial logs: `[TX] payload=…` marks transmissions, `[RX] …` shows decoded inbound packets, and `[FWD] …` indicates a relayed frame.  
- Tag firmware releases that go to the field and keep binaries for rollback.  
- The built-in LED flashes after each successful transmit; no blink means the LoRa handshake failed.

---

## Possible Extensions

- Add OTA update hooks or signed firmware if tamper evidence is required.  
- Tie into alternate transports (Bluetooth provisioning, Wi-Fi debug bridges).  
- Enable additional sensors or derived metrics (AQI, fuel moisture indices).  
- Expand the CRC cache size or persist hashes if your mesh needs longer duplicate suppression windows.  
- Publish maintenance telemetry (uptime, duty cycle stats, supply voltage minima).
