"""Send synthetic FireLord DATA packets into Postgres for testing the logger schema."""

import argparse
import os
import random
import time
from typing import List

from .logger import Config, connect_db, ensure_schema, store_packet


def build_packets(count: int, node_id: int, start_ts: int) -> List[str]:
    lines = []
    for i in range(count):
        device_ts = start_ts + i
        temp_cx100 = int((20.0 + random.uniform(-3, 3)) * 100)
        humidity = round(40 + random.uniform(-5, 5), 1)
        co2 = int(450 + random.uniform(0, 50))
        pressure = int(101325 + random.uniform(-500, 500))
        voc = random.randint(10, 50)
        smoke = random.randint(10, 50)
        flags = 0
        line = f"DATA {node_id} 1 {device_ts} {temp_cx100} {humidity} {co2} {pressure} {voc} {smoke} {flags}"
        lines.append(line)
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, default=5, help="How many packets to send")
    parser.add_argument("--node-id", type=int, default=101, help="Node ID to use")
    parser.add_argument("--start-ts", type=int, default=int(time.time()), help="Starting device_ts")
    parser.add_argument("--repeat-first", action="store_true", help="Send the first packet twice to exercise dedup")
    args = parser.parse_args()

    cfg = Config(
        serial_port=os.getenv("SERIAL_PORT", "/dev/ttyACM0"),
        baud_rate=int(os.getenv("BAUD_RATE", "115200")),
        db_host=os.getenv("DB_HOST", "localhost"),
        db_port=int(os.getenv("DB_PORT", "5432")),
        db_name=os.getenv("DB_NAME", "firelord"),
        db_user=os.getenv("DB_USER", "firelord"),
        db_password=os.getenv("DB_PASSWORD", "firelord"),
        reconnect_delay_seconds=float(os.getenv("RECONNECT_DELAY_SECONDS", "3")),
    )

    conn = connect_db(cfg)
    ensure_schema(conn)

    lines = build_packets(args.count, args.node_id, args.start_ts)
    if args.repeat_first and lines:
        lines.insert(1, lines[0])

    inserted = 0
    for line in lines:
        parts = line.split()
        packet = {
            "node_id": int(parts[1]),
            "version": int(parts[2]),
            "device_ts": int(parts[3]),
            "temp_c": int(parts[4]) / 100.0,
            "humidity_pct": float(parts[5]),
            "co2_ppm": int(parts[6]),
            "pressure_pa": int(parts[7]),
            "voc_raw": int(parts[8]),
            "smoke_raw": int(parts[9]),
            "flags": int(parts[10]),
        }
        if store_packet(conn, packet, line):
            inserted += 1
            print(f"Inserted: {line}")
        else:
            print(f"Duplicate skipped: {line}")

    print(f"Done. Inserted {inserted}/{len(lines)} packets.")


if __name__ == "__main__":
    main()
