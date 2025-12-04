#!/usr/bin/env bash
set -euo pipefail

# Setup virtual environment and install dependencies for the FireLord USB->Postgres logger.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f .env ]; then
  echo "Missing .env. Copying from .env.example ..."
  cp .env.example .env
  echo "Edit .env to match your serial port and database settings." >&2
fi

python -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt

echo "Setup complete. Start Postgres with 'docker compose up -d' then run start.sh to launch the logger." 
