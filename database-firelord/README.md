# database-firelord

Simple split setup: Postgres runs in Docker, while a host-based Python script reads the FireLord base-station USB serial output and writes parsed packets into the database over TCP (localhost by default).

## What it does
- Watches serial for lines shaped like `DATA <nodeId> <ver> <ts> <tempCx100> <humidity> <co2> <pressure> <voc> <smoke> <flags>`.
- Converts temperature from centi-degrees to °C, keeps the rest as provided, and inserts into Postgres.
- Auto-reconnects on serial or database hiccups; ignored lines are logged at `DEBUG`.
- Can target a remote Postgres instance by changing `DB_HOST` (e.g., centralized ingest).
- Deduplicates packets using a SHA-256 hash of the raw `DATA` line (unique index on `packet_hash`); duplicates are skipped quietly.
- Optional web viewer shows recent packets and allows manual clear.

## Quick start (scripts)
- **Linux/macOS:** `./setup.sh` (creates .env if missing, sets up venv), then `./start.sh` (brings up Postgres via Docker when DB_HOST is local and runs the logger).
- **Windows:** Use PowerShell `start.ps1` (after running `setup.sh` once from WSL or manually creating `.venv`/`.env`). `start.ps1 -SkipCompose` skips local Postgres if pointing at a remote DB.

Stop the DB with `docker compose down`. Data persists in `data/db` (mounted to the Postgres data dir). The logger runs on the host so it can see the USB serial device directly.
If `DB_HOST` is not localhost/127.0.0.1`, the start scripts skip bringing up the local Postgres container (assumes you're pointing at a remote DB).
Note: the Postgres container uses `PGDATA=/var/lib/postgresql/data/pgdata` to avoid clashing with hidden files in the bind mount; keep the `data/db` directory clean (the `.gitkeep` in the parent is fine).

## Quick start (manual)
1) Copy and edit environment defaults:

```bash
cd database-firelord
cp .env.example .env  # adjust SERIAL_PORT / DB_* as needed
```

2) Start the database (only Postgres is containerized):

```bash
docker compose up -d
```

3) Install Python deps on the host and run the logger:

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
set -a; source .env; set +a  # export env vars for this shell
python -m firelord_logger
```

## Configuration (env vars)
- `SERIAL_PORT` (default `/dev/ttyACM0`): USB serial path for the base-station node. On Windows use the COM name (e.g., `COM3`) shown in Device Manager.
- `BAUD_RATE` (default `115200`): Serial baud.
- `LOG_LEVEL` (default `INFO`): Set `DEBUG` to see ignored/garbled lines.
- `RECONNECT_DELAY_SECONDS` (default `3`): Back-off before retrying serial or DB.
- `DB_HOST` (default `localhost`): Where Postgres is reachable; can be remote for centralized ingest.
- `DB_PORT` (default `5432`): Postgres port.
- `DB_NAME`, `DB_USER`, `DB_PASSWORD` (default `firelord`): Database credentials used by both Postgres and the logger.

## Checking the data
From the host (with Postgres running):

```bash
psql "postgresql://$DB_USER:$DB_PASSWORD@$DB_HOST:$DB_PORT/$DB_NAME" \
  -c "SELECT id, node_id, device_ts, temp_c, humidity_pct, co2_ppm, flags, recorded_at FROM packets ORDER BY id DESC LIMIT 5;"
```

Fields stored: node_id, version, device_ts (as emitted by the node), temp_c (float), humidity_pct, co2_ppm, pressure_pa, voc_raw, smoke_raw, flags, raw_line, and recorded_at (ingest timestamp).

## Testing without hardware
Run the synthetic sender to push fake `DATA` packets directly into Postgres (uses the same DB env vars):

```bash
# Linux/macOS (after setup.sh/start.sh so DB is up)
python -m firelord_logger.test_sender --count 5 --node-id 900 --repeat-first

# Windows PowerShell
Set-Location database-firelord
.\start.ps1   # in another terminal to keep DB up, or run docker compose up -d once
python -m firelord_logger.test_sender --count 5 --node-id 900 --repeat-first
```

The `--repeat-first` flag injects a duplicate packet to confirm deduplication (it will print “Duplicate skipped”).

## Web viewer
Simple, un-authenticated viewer (use only on trusted networks):

```bash
# after setup/start so dependencies and DB are ready
export VIEWER_PORT=8000  # optional
python -m firelord_logger.viewer
```

Then visit http://localhost:8000 to see:
- Left: recent raw `DATA` lines with timestamps.
- Right: decoded fields table.
- “Clear all data” button (prompts for confirmation and truncates the table).

## Notes
- Ensure your user can access the serial device on the host (e.g., dialout/tty group on Linux).
- If pointing at a remote database, secure credentials and connectivity appropriately (TLS, firewall, strong passwords).
 - Windows users: prefer PowerShell `start.ps1`; the previous batch files were removed due to cmd parsing quirks.
