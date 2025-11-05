# Electronics Design Guide

The KiCad project in this directory defines the sensor carrier board used by FireLord. It is suitable for small fabrication runs and straightforward to adapt for alternate sensors or power stages.

---

## Project Layout

```
Pressure_Sensor/
└── Pressure_Sensor_Board/
    ├── Pressure_Sensor_Board.kicad_pro
    ├── Pressure_Sensor_Board.kicad_sch
    ├── Pressure_Sensor_Board.kicad_pcb
    ├── PressureSensor.pretty/
    └── fp-lib-table
```

Open the `.kicad_pro` file in KiCad 7+. If footprints appear missing, point KiCad to the bundled `PressureSensor.pretty` library.

---

## Circuit Highlights

- Seeeduino XIAO header exposing I²C, UART, analog, and power rails.  
- JST or locking connectors for CO, smoke, VOC, and other sensor daughterboards.  
- SCD40 and MPL115A2 wired on a shared I²C bus with 400 kHz pull-ups.  
- Supercapacitor pads with balancing resistors and space for optional regulators.  
- Test pads for 3.3 V, ground, UART TX/RX, and reset.

Trace widths and component spacing were selected for easy hand assembly.

---

## Fabrication Workflow

1. Run ERC/DRC and resolve any violations.  
2. Generate Gerbers, drills, and pick-and-place files; archive each release (e.g., `fab/2025-03-RevA/`).  
3. Order 1.6 mm FR-4, 1 oz copper boards from your preferred supplier.  
4. Assemble SMD parts first, then supercapacitors and connectors.  
5. Verify I²C devices with the firmware scan and confirm LoRa UART signalling before enclosure install.

---

## Customization Notes

- Update footprints and BOM items when swapping sensors; confirm mechanical clearance against the enclosure model.  
- Replace the supercapacitor block with LiFePO₄ hardware if longer runtime is required.  
- Add extra headers for status LEDs, buttons, or expansion harnesses.  
- Introduce an SMA footprint with controlled impedance if you need external antennas.

Record every change with a new schematic and PCB revision.

---

## Footprint Library

`PressureSensor.pretty` contains enlarged pads for hand soldering. If you fork the design, duplicate the library and keep naming conventions (e.g., `FLD_SENSOR_*`) so scripts resolve correctly. Export 3D models as needed for enclosure fit checks.

---

## Additional Notes

- BOM reference: LoRa radio $16.90, Seeeduino $5.40, SCD40 $17.95, supercapacitors $17.50 each, remaining sensors $5–$10.  
- Environmental targets: IP55 enclosure, −30 °C survivability for 30 min, and a 200 mm maximum dimension to fit the printed housing.  
- Common risks: delayed component deliveries, insufficient node density, and inadequate solar exposure; plan mitigations during layout and procurement.
