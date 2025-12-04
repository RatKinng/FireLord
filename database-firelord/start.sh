#!/usr/bin/env bash
set -euo pipefail

# Start Postgres (Docker) and run the FireLord USB->Postgres logger on the host.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f .env ]; then
  echo "Missing .env. Run setup.sh first to create it." >&2
  exit 1
fi

if [ ! -d .venv ]; then
  echo "Missing virtual environment (.venv). Run setup.sh first." >&2
  exit 1
fi

set -a
source .env
set +a

if [ "${DB_HOST:-localhost}" = "localhost" ] || [ "${DB_HOST:-localhost}" = "127.0.0.1" ]; then
  echo "Starting Postgres via docker compose..."
  docker compose up -d
else
  echo "DB_HOST is ${DB_HOST}; skipping local docker compose since database is remote."
fi

echo "Launching logger..."
source .venv/bin/activate
exec python -m firelord_logger
