# FireLord Sensor Network Platform

FireLord is an open hardware reference for low-cost, peer-to-peer sensor meshes. The repository captures mechanical models, firmware helpers, PCB files, and planning artifacts produced during an Alaskan field concept so future teams can reuse or adapt the design.

---

## Intended Audience

- Municipal and resource agencies that need rapidly deployable sensor coverage.
- Community groups and educators building inexpensive environmental monitors.
- Integrators evaluating FireLord as a foundation for broader sensing programs.

---

## Repository Map

| Path | Contents |
| --- | --- |
| [`device-firmware/`](device-firmware/README.md) | Arduino-oriented helpers for sensor sampling and LoRa communication. |
| [`device-case/`](device-case/README.md) | 3D models, print notes, and assembly guidance for the field enclosure. |
| [`pcb-and-schematics/`](pcb-and-schematics/README.md) | KiCad project for the carrier board and custom footprints. |

---

## System Overview

1. **Field nodes** collect local sensor data, flag anomalies, and rebroadcast peer packets.  
2. **Base stations** log every packet via a USB LoRa interface and forward data when backhaul is available.  
3. **Optional cloud services** expose data to stakeholders or downstream tooling.

The mesh design keeps infrastructure light: any node can relay data, and a single active base station is sufficient.

---

## Quick Start

1. Review the operating targets below (range, duty cycle, enclosure limits) and adjust for your field conditions.  
2. Select components from the baseline bill of materials; plan on roughly $75 per node at 100-unit scale.  
3. Manufacture or print hardware following the subsystem READMEs.  
4. Load the firmware scaffold, confirm sensor and LoRa links, and tailor packet contents.  
5. Deploy nodes so each device reaches two peers or a base station, then validate end-to-end logging.

Use the risk guidance below while planning; the most common issues are supply delays, isolated nodes, and inadequate solar input.

---

## Operating Targets

- Peer-to-peer LoRa range: design for non-line-of-sight coverage of at least 500 m between nodes; verify in your terrain.  
- Reporting cadence: transmit sensor packets no less frequently than every 60 s unless power constraints dictate a longer interval.  
- Receive availability: keep the radio in a ready state to forward peer traffic continuously.  
- Data bandwidth: maintain ≥ 1.2 KB/s (9600 baud) for device-to-device links.  
- Power: average receive-ready draw ≤ 100 mW with duty-cycled heaters; energy storage provided by supercapacitors only.  
- Mechanical envelope: keep external dimensions within 200 mm per axis to fit the reference enclosure.  
- Environmental durability: target IP55 ingress protection and verified operation down to −30 °C for at least 30 min.  
- Radio spectrum: operate in the regional sub-GHz LoRa band (e.g., 902–928 MHz in North America) with a fixed-length header and byte-structured frame format.  
- Bill of materials: $75 per node is achievable at 100-unit quantities using the parts listed below.

---

## Deployment Notes

- **Topology**: Maintain overlapping coverage; document approved node spacing before field work.  
- **Placement**: Mount above snow or vegetation, shield cables from wildlife, and orient vents downward.  
- **Backhaul**: Configure the base station to cache indefinitely; cloud transfer is optional.  
- **Operations**: Schedule inspections ahead of seasonal activities (e.g., controlled burns or tourism peaks).

---

## Hardware Snapshot

| Component | Key details | Mount style | Unit cost (USD, 2-unit qty) | Remarks |
| --- | --- | --- | --- | --- |
| [LoRa module (Reyax RYLR998)](https://www.digikey.com/en/products/detail/reyax/RYLR998/22078252) | 3.3 V P2P radio | Through-hole module | 16.90 | Ships on a carrier with 2.54 mm headers; can be socketed or wired directly without reflow. |
| [MCU (Seeeduino XIAO SAMD21)](https://www.digikey.com/en/products/detail/seeed-technology-co-ltd/102010328/11506471) | Arduino-compatible controller | Castellated SMD | 5.40 | Hand-solderable to the XIAO footprint; DIP adapters are available if you prefer header pins. |
| [CO₂/temp/humidity sensor (Sensirion SCD40)](https://www.digikey.com/en/products/detail/sensirion-ag/SCD40-D-R2/13684008) | I²C environmental sensing | LGA SMD | 17.95 | Use a breakout like the [SparkFun SEN-18360](https://www.sparkfun.com/products/18360) when avoiding reflow. |
| [CO sensor (MQ-7)](https://www.digikey.com/en/products/detail/olimex-ltd/SNS-MQ7/21662604) | Analog gas sensor | Through-hole | 5.65 | Duty-cycle the heater to save power. |
| [Pressure sensor (NXP MPL115A2)](https://www.digikey.com/en/products/detail/nxp-usa-inc/MPL115A2ST1/16538791) | I²C barometer | LGA SMD | 4.75 | Choose a breakout (Adafruit 992 or similar) if assembling without reflow. |
| [Smoke sensor (DFRobot SEN0570)](https://www.digikey.com/en/products/detail/dfrobot/SEN0570/24611153) | Analog particle indicator | Module w/2.54 mm header | 4.90 | Mount behind vent mesh. |
| [VOC sensor (DFRobot SEN0566)](https://www.digikey.com/en/products/detail/dfrobot/SEN0566/24611145) | Analog VOC indicator | Module w/2.54 mm header | 4.90 | Co-locate with smoke sensor. |
| [Supercapacitor (Tecate TPLH-2R7-800)](https://www.digikey.com/en/products/detail/tecate-group/TPLH-2R7-800PS35X71/21378858) | 2.7 V 800 F storage | Radial through-hole | 17.50 (each) | Use two in series with balancing. |
| [USB LoRa gateway (LoStik-class)](https://www.digikey.com/en/products/detail/sb-components-ltd/SKU26586/18723792) | USB-to-LoRa bridge | External | 46.19 | Serves as base-station interface. |
| [Solar panel (Seeed 1 W)](https://www.mouser.com/ProductDetail/Seeed-Studio/313070001) | 5 V nominal, 1 W | Leaded | 11.95 | Size array for local insolation. |
| [Connectors, cabling, misc.](https://www.amazon.com/100Pcs-Connector-Include-Female-Ph2-0mm/dp/B0DMW21LWS) | JST harnesses, fasteners | Through-hole / pre-crimped | ~10.00 | Include spare gaskets and glands. |

Estimated total: ≈ $75 per field node at 100-unit scale. Choosing breakout boards for the SMD-only sensors raises cost slightly but simplifies assembly when reflow is unavailable.

---

## Power Profile

- 283 mW: receive-ready state with high-draw sensors active.  
- 116 mW: duty-cycled average with smoke/VOC heaters at 5 % duty.  
- 600 mAh equivalent storage supports ~7 h at peak load and ~17 h at averaged load.

Adjust sampling cadence, heater duty cycles, or panel capacity to meet local endurance goals.

---

## Risk Summary

| Theme | Mitigation |
| --- | --- |
| Part delays | Order early, maintain alternates, record substitutions. |
| Node isolation | Pre-survey install sites, document minimum spacing, train field crews. |
| Power loss | Site panels in clear sun, inspect seasonally, log low-voltage events. |
| Sensor drift | Implement firmware sanity checks; schedule recalibration windows. |
| Base station downtime | Provide backup power and confirm local logging before field work. |

Document mitigations and revisit them seasonally to keep deployments stable.

---

## Extending the Platform

- Add routing intelligence, adaptive transmit power, or health beacons for larger meshes.  
- Integrate SMS, dashboard, or API publishing pipelines.  
- Incorporate soil probes, weather stations, or perimeter sensors.  
- Package sensor pods for quick swap service.  
- Expand packet signing or integrity checks for regulated deployments.

---

## Prototype Artifacts

- Work breakdowns and PERT charts outline the original build sequence.  
- Firmware helpers (`I2C_COMMS`, `UART_COMMS`) provide safe defaults for sensor and radio access.  
- Mechanical CAD documents enclosure iterations and tolerance tests.

Use these as reference when creating production-ready variants.
