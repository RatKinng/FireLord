# Device Enclosure Guide

This directory holds the enclosure CAD for the FireLord node. Models target desktop FDM printers and support IP55-style sealing.

---

## File Index

| File | Notes |
| --- | --- |
| `FireLordCase.f3d` | Fusion 360 source. |
| `FireLordCase.step` | Neutral CAD export. |
| `FireLordCase.3mf` | Full assembly print plate. |
| `FireLordCaseBody.*`, `FireLordCaseLid.*`, `FireLordCaseGasket.*` | Individual components. |
| `FireLordCaseMesh.*` | Vent cover for airflow. |
| `case.scad`, `size-tests.scad` | OpenSCAD parametric scripts and tolerance checks. |

---

## Print Defaults

- Material: PETG, ASA, PC, or other outdoor-tolerant material; PLA only for indoor pilots. Gasket should be TPU or other flexible filament.  
- Layer height: 0.2 mm (0.16 mm for smoother gasket lands).  
- Perimeters: ≥ 3 (for sufficient water-proofing).  
- Infill: 20 % grid/gyroid for the body; solid for the gasket supports.  
- Supports: Off for body/lid/gasket; trees for the mesh.  
- Orientation: Body upright, lid upside-down, mesh and gasket flat.

Run `size-tests.scad` before large batches.

---

## Assembly Steps

1. **Gasket** – Print in TPU or cut 2 mm silicone, then dry-fit the lid channel.  
2. **Electronics** – Mount the PCB on standoffs with stainless M3 hardware; align sensors with vent openings.  
3. **Cabling** – Route solar and antenna leads through side ports, sealing with glands or potting as required; strain-relief internal wiring.  
4. **Closure** – Seat the gasket, tighten lid screws diagonally, confirm even compression.  
5. **Finish** – Install the mesh cover and label the enclosure with device ID and install date.

---

## Environmental Notes

- **Cold**: Add insulating foam around supercapacitors and orient vents downward to shed ice.  
- **Moisture**: Apply conformal coating and include desiccant in humid regions.  
- **Wildlife**: Mount ≥ 3 m high where large animals are active; guard solar panels with metal frames.  
- **Heat or flame**: The case is not fire-rated—document safe-service guidelines for field teams.

---

## CAD Adjustments

- Modify `case.scad` for alternate sensor cut-outs or mounting features.  
- Extend the lid if an external antenna requires additional clearance.  
- Add hinged or quick-release hardware if frequent access is expected.

Track revisions alongside firmware changes to keep assemblies synchronized.

---

## Deployment Reminders

- Assemble indoors and transport with desiccant packs.  
- Use torque-limited drivers to prevent cracking.  
- Record install coordinates and photos.  
- Inspect critical sites before seasonal operations to clear vents and verify seals.
