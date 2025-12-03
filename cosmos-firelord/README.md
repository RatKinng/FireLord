# FireLord COSMOS Stack

This folder packages a ready-to-run OpenC3 COSMOS instance for FireLord field testing. It mounts the FireLord LoRa plugin, exposes the COSMOS web UI locally, and includes a bridge profile for piping radio traffic from a base-station FireLord node (USB serial) into COSMOS tools.

## What's Here

- `compose.yaml` and `.env`: container stack and defaults pinned to OpenC3 6.9.2 in local mode. Update the secrets in `.env` before exposing beyond localhost.
- `openc3-cosmos-lora/`: plugin source plus the TCP bridge profile (`bridge.txt`) that relays a LoRa serial link into COSMOS.
- `plugins/DEFAULT/openc3-cosmos-lora/`: the packaged `openc3-cosmos-lora-1.0.0.gem` with an instance preconfigured for target `LORA` on `host.docker.internal:2950`.
- `openc3.sh` / `openc3.bat`: helper wrapper for running, stopping, and managing COSMOS containers.

## Quick Start (Local)

1) Install Docker with Compose and add your user to the Docker group (or run with sudo).  
2) From the repo root: `cd cosmos-firelord`  
3) Edit `.env` to rotate all placeholder passwords and adjust ports if 2900/2943 collide locally.  
4) Start COSMOS:  
   - macOS/Linux: `./openc3.sh run`  
   - Windows: `openc3.bat run`  
   Wait for the `openc3-init` container to finish, then open http://localhost:2900.

## Hook Up the Base-Station Node

The FireLord plugin expects a TCP feed on port `2950`. Use the included bridge profile to forward a FireLord device running `BASE_STATION_MODE=true` (receive-only, sensorless) into that port:

```bash
cd cosmos-firelord
./openc3.sh cli bridge openc3-cosmos-lora/bridge.txt \
  write_port_name=/dev/ttyACM0 \
  read_port_name=/dev/ttyACM0 \
  baud_rate=115200 \
  router_port=2950
```

- COSMOS “attempting to connect” here is only the TCP socket to this bridge, not RF link state. If you see it, the bridge is not reachable.
- Free the port first. `compose.yaml` currently maps `openc3-minio` to `127.0.0.1:2950`, which blocks the bridge. Comment that mapping or pick another port and update `plugin.txt`, `plugin_instance.json`, and `bridge.txt` to match.
- Run the bridge where the USB device is available. The helper command runs inside a container; on Windows/WSL it is often easier to run a host-side serial→TCP forwarder on the chosen port instead.

- Change `/dev/ttyACM0` to the COM/tty device for your base-station node.  
- Adjust `router_listen_address` if COSMOS is running on another host.  
- The plugin instance in `plugins/DEFAULT` points to `host.docker.internal:2950`, so the bridge must run on the same machine as Docker.

## Using the FireLord LORA Target

- Target name: `LORA` (configurable via `plugins/DEFAULT/openc3-cosmos-lora/plugin_instance.json`).  
- Interface: TCP client to the local bridge (port 2950).  
- Telemetry reflects the 23-byte FireLord sample payload (version, node ID, uptime seconds, temperature x100 C, humidity x100 %, CO2 ppm, pressure x10 hPa, VOC/SMOKE ADC counts, status flags, CRC32). The base-station firmware emits a `DATA …` line for every validated packet that can be parsed downstream; nodes are TX-only, so no commands are defined yet.  
- Screens/procedures live under `openc3-cosmos-lora/targets/LORA/` and will appear in COSMOS once the plugin is loaded.

## Updating the Plugin

1) Edit the plugin source under `openc3-cosmos-lora/` (e.g., add real telemetry items).  
2) Build a new gem: `./openc3.sh cli rake build VERSION=1.0.1` from inside `openc3-cosmos-lora/`.  
3) Replace the gem in `plugins/DEFAULT/openc3-cosmos-lora/` and update `plugin_instance.json` if the filename changes.  
4) Reload the plugin in COSMOS (Admin → Plugins) or via CLI:  
   `./openc3.sh cli load plugins/DEFAULT/openc3-cosmos-lora/openc3-cosmos-lora-1.0.1.gem DEFAULT plugins/DEFAULT/openc3-cosmos-lora/plugin_instance.json force`.

## Notes

- Keep the `.env` credentials private; they are mounted directly into containers.  
- The `plugins/DEFAULT` directory is bind-mounted and will be modified at runtime; commit only intentional changes.  
- For networks beyond localhost, enable TLS by swapping `openc3-traefik/traefik.yaml` for one of the SSL variants and providing certificates.
