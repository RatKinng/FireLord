# FireLord LoRa COSMOS Plugin

This plugin packages the FireLord LoRa target for OpenC3 COSMOS. It ships with a TCP client interface that expects a host-side bridge from a FireLord base-station node (USB serial) and includes stub command/telemetry definitions you can replace with the real packet map.

## Structure

- `plugin.txt`: declares target `LORA` and a TCP client pointed at `host.docker.internal:2950`. The value for `lora_target_name` is templated so it can be overridden when loading the plugin.  
- `bridge.txt`: host-side serial↔TCP bridge. Run it on the same machine as Docker so the container can reach `host.docker.internal:2950`.  
- `targets/LORA/`: placeholder COSMOS target (cmd/tlm, screens, procedures, and a helper `lib/lora.py`). Swap these with real items to visualize FireLord traffic.
- `openc3-cosmos-lora-1.0.0.gem`: prebuilt gem consumed by `plugins/DEFAULT/openc3-cosmos-lora/plugin_instance.json`.

## Running the Bridge

From the `cosmos-firelord` directory:

```bash
./openc3.sh cli bridge openc3-cosmos-lora/bridge.txt \
  write_port_name=/dev/ttyACM0 \
  read_port_name=/dev/ttyACM0 \
  baud_rate=115200 \
  router_port=2950
```

- COSMOS “attempting to connect” is only the TCP socket to this bridge, not RF link state. If you see that message, the bridge is unreachable.
- Ensure the port is free. In this stack `compose.yaml` maps `openc3-minio` to `127.0.0.1:2950`; comment that mapping or pick a new port and update `plugin.txt`, `plugin_instance.json`, and `bridge.txt` together.
- Run the bridge where the base-station device (FireLord node with `BASE_STATION_MODE=true`) is accessible over USB serial. The helper command runs in a container; on Windows/WSL you may need a host-side serial→TCP forwarder listening on the chosen port instead.

- Match the `router_port` to `plugin.txt` (2950 by default).  
- Update `write_port_name`/`read_port_name` for your COM/tty device and parity/flow control if needed; the base-station firmware uses 115200 baud.  
- Set `router_listen_address=0.0.0.0` only when COSMOS runs on another host and you trust the network.

## Editing and Rebuilding

1) Update the target definitions under `targets/LORA/` and any supporting utilities.  
2) Bump the gem version and rebuild from this folder:  
   `./openc3.sh cli rake build VERSION=1.0.1`  
3) Copy the new gem into `plugins/DEFAULT/openc3-cosmos-lora/` and update `plugin_instance.json` so COSMOS loads the new filename.  
4) Reload the plugin through the COSMOS Admin > Plugins page or `./openc3.sh cli load ...` (see `cosmos-firelord/README.md`).

General, upstream plugin boilerplate now lives in `README.upstream.md` for reference.
