# database-firelord

Lightweight Dockerized listener that tails the FireLord base-station USB serial output and stores parsed `DATA` lines into a local SQLite database.

## What it does
- Opens the base-station USB serial device and watches for lines shaped like `DATA <nodeId> <ver> <ts> <tempCx100> <humidity> <co2> <pressure> <voc> <smoke> <flags>`.
- Converts temperature from centi-degrees to °C, keeps the rest as provided, and saves each packet into `data/firelord.db`.
- Auto-reconnects on serial hiccups; logs ignored lines for troubleshooting at `DEBUG` level.

## Run it
1) Copy the sample env file and adjust the serial device path if needed:

```bash
cd database-firelord
cp .env.example .env  # edit SERIAL_PORT if your device is not /dev/ttyACM0
```

2) Build and start the logger (Compose V2 syntax shown):

```bash
docker compose up --build
```

A `data/` folder is mounted from the host so the SQLite file persists across restarts. Stop with `Ctrl+C` or `docker compose down`.

## Configuration
- `SERIAL_PORT` (default `/dev/ttyACM0`): USB serial path for the base-station node. The compose file also passes this device through to the container.
- `BAUD_RATE` (default `115200`): Serial baud.
- `LOG_LEVEL` (default `INFO`): Set to `DEBUG` to see ignored/garbled lines.
- `RECONNECT_DELAY_SECONDS` (default `3`): Back-off before retrying the serial port after an error.

## Checking the data
Use SQLite directly on the host-mounted file:

```bash
sqlite3 data/firelord.db "SELECT id, node_id, device_ts, temp_c, humidity_pct, co2_ppm, flags, recorded_at FROM packets ORDER BY id DESC LIMIT 5;"
```

Fields stored: node_id, version, device_ts (as emitted by the node), temp_c (float), humidity_pct, co2_ppm, pressure_pa, voc_raw, smoke_raw, flags, raw_line, and recorded_at (ingest timestamp).

## Notes
- The container assumes you have permission to access the serial device; on some systems you may need to add your user to the appropriate dialout/tty group or run Docker with elevated device access.
- Only `DATA` lines are persisted; other serial chatter is ignored so the database stays clean.
