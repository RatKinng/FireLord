import logging
import os
import sqlite3
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional

import serial


@dataclass
class Config:
    serial_port: str
    baud_rate: int
    db_path: str
    reconnect_delay_seconds: float


def load_config() -> Config:
    return Config(
        serial_port=os.getenv("SERIAL_PORT", "/dev/ttyACM0"),
        baud_rate=int(os.getenv("BAUD_RATE", "115200")),
        db_path=os.getenv("DB_PATH", "/data/firelord.db"),
        reconnect_delay_seconds=float(os.getenv("RECONNECT_DELAY_SECONDS", "3")),
    )


def init_logging() -> None:
    log_level = os.getenv("LOG_LEVEL", "INFO").upper()
    logging.basicConfig(
        level=getattr(logging, log_level, logging.INFO),
        format="%(asctime)s %(levelname)s %(message)s",
    )


def init_db(db_path: str) -> sqlite3.Connection:
    Path(db_path).parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(db_path, detect_types=sqlite3.PARSE_DECLTYPES, timeout=5)
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS packets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            recorded_at TEXT DEFAULT CURRENT_TIMESTAMP,
            node_id INTEGER,
            version INTEGER,
            device_ts INTEGER,
            temp_c REAL,
            humidity_pct REAL,
            co2_ppm INTEGER,
            pressure_pa INTEGER,
            voc_raw INTEGER,
            smoke_raw INTEGER,
            flags INTEGER,
            raw_line TEXT
        );
        """
    )
    conn.execute("CREATE INDEX IF NOT EXISTS idx_packets_node ON packets(node_id);")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_packets_ts ON packets(device_ts);")
    conn.commit()
    return conn


def parse_data_line(line: str) -> Optional[Dict[str, object]]:
    parts = line.strip().split()
    if len(parts) != 11 or parts[0] != "DATA":
        return None

    try:
        node_id = int(parts[1])
        version = int(parts[2])
        device_ts = int(parts[3])
        temp_c = int(parts[4]) / 100.0
        humidity_pct = float(parts[5])
        co2_ppm = int(parts[6])
        pressure_pa = int(parts[7])
        voc_raw = int(parts[8])
        smoke_raw = int(parts[9])
        flags = int(parts[10], 0)
    except ValueError:
        return None

    return {
        "node_id": node_id,
        "version": version,
        "device_ts": device_ts,
        "temp_c": temp_c,
        "humidity_pct": humidity_pct,
        "co2_ppm": co2_ppm,
        "pressure_pa": pressure_pa,
        "voc_raw": voc_raw,
        "smoke_raw": smoke_raw,
        "flags": flags,
    }


def store_packet(conn: sqlite3.Connection, packet: Dict[str, object], raw_line: str) -> None:
    conn.execute(
        """
        INSERT INTO packets (
            node_id, version, device_ts, temp_c, humidity_pct, co2_ppm,
            pressure_pa, voc_raw, smoke_raw, flags, raw_line
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        """,
        (
            packet["node_id"],
            packet["version"],
            packet["device_ts"],
            packet["temp_c"],
            packet["humidity_pct"],
            packet["co2_ppm"],
            packet["pressure_pa"],
            packet["voc_raw"],
            packet["smoke_raw"],
            packet["flags"],
            raw_line,
        ),
    )
    conn.commit()


def read_loop(config: Config, conn: sqlite3.Connection) -> None:
    while True:
        try:
            with serial.Serial(config.serial_port, config.baud_rate, timeout=2) as ser:
                logging.info(
                    "Connected to %s at %s baud; writing to %s",
                    config.serial_port,
                    config.baud_rate,
                    config.db_path,
                )

                for raw_bytes in iter(ser.readline, b""):
                    if not raw_bytes:
                        continue

                    line = raw_bytes.decode(errors="ignore").strip()
                    if not line:
                        continue

                    packet = parse_data_line(line)
                    if not packet:
                        logging.debug("Ignoring non-DATA line: %s", line)
                        continue

                    store_packet(conn, packet, line)
                    logging.info(
                        "Stored packet from node %s (fw %s) ts=%s temp=%.2fC humidity=%.1f%% co2=%s ppm",
                        packet["node_id"],
                        packet["version"],
                        packet["device_ts"],
                        packet["temp_c"],
                        packet["humidity_pct"],
                        packet["co2_ppm"],
                    )
        except serial.SerialException as exc:
            logging.warning(
                "Serial error '%s'; retrying in %.1f s", exc, config.reconnect_delay_seconds
            )
            time.sleep(config.reconnect_delay_seconds)
        except sqlite3.Error as exc:
            logging.error("SQLite error '%s'; backing off before retry", exc)
            time.sleep(config.reconnect_delay_seconds)
        except KeyboardInterrupt:
            logging.info("Stopping listener (keyboard interrupt)")
            break


def main() -> None:
    init_logging()
    config = load_config()
    conn = init_db(config.db_path)
    read_loop(config, conn)


if __name__ == "__main__":
    main()
