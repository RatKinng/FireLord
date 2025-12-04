import logging
import os
import time
from dataclasses import dataclass
from typing import Dict, Optional

import hashlib
import psycopg2
import serial


@dataclass
class Config:
    serial_port: str
    baud_rate: int
    db_host: str
    db_port: int
    db_name: str
    db_user: str
    db_password: str
    reconnect_delay_seconds: float


def load_config() -> Config:
    return Config(
        serial_port=os.getenv("SERIAL_PORT", "/dev/ttyACM0"),
        baud_rate=int(os.getenv("BAUD_RATE", "115200")),
        db_host=os.getenv("DB_HOST", "localhost"),
        db_port=int(os.getenv("DB_PORT", "5432")),
        db_name=os.getenv("DB_NAME", "firelord"),
        db_user=os.getenv("DB_USER", "firelord"),
        db_password=os.getenv("DB_PASSWORD", "firelord"),
        reconnect_delay_seconds=float(os.getenv("RECONNECT_DELAY_SECONDS", "3")),
    )


def init_logging() -> None:
    log_level = os.getenv("LOG_LEVEL", "INFO").upper()
    logging.basicConfig(
        level=getattr(logging, log_level, logging.INFO),
        format="%(asctime)s %(levelname)s %(message)s",
    )


def connect_db(config: Config) -> psycopg2.extensions.connection:
    conn = psycopg2.connect(
        host=config.db_host,
        port=config.db_port,
        dbname=config.db_name,
        user=config.db_user,
        password=config.db_password,
    )
    conn.autocommit = True
    return conn


def ensure_schema(conn: psycopg2.extensions.connection) -> None:
    with conn.cursor() as cur:
        cur.execute(
            """
            CREATE TABLE IF NOT EXISTS packets (
                id SERIAL PRIMARY KEY,
                recorded_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
                node_id INTEGER,
                version INTEGER,
                device_ts INTEGER,
                temp_c DOUBLE PRECISION,
                humidity_pct DOUBLE PRECISION,
                co2_ppm INTEGER,
                pressure_pa INTEGER,
                voc_raw INTEGER,
                smoke_raw INTEGER,
                flags INTEGER,
                packet_hash TEXT,
                raw_line TEXT
            );
            """
        )
        cur.execute("ALTER TABLE packets ADD COLUMN IF NOT EXISTS packet_hash TEXT;")
        cur.execute("CREATE INDEX IF NOT EXISTS idx_packets_node ON packets(node_id);")
        cur.execute("CREATE INDEX IF NOT EXISTS idx_packets_ts ON packets(device_ts);")
        cur.execute(
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_packets_hash ON packets(packet_hash);"
        )


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


def compute_packet_hash(raw_line: str) -> str:
    return hashlib.sha256(raw_line.encode("utf-8")).hexdigest()


def store_packet(
    conn: psycopg2.extensions.connection, packet: Dict[str, object], raw_line: str
) -> bool:
    packet_hash = compute_packet_hash(raw_line)
    with conn.cursor() as cur:
        cur.execute(
            """
            INSERT INTO packets (
                node_id, version, device_ts, temp_c, humidity_pct, co2_ppm,
                pressure_pa, voc_raw, smoke_raw, flags, packet_hash, raw_line
            ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (packet_hash) DO NOTHING;
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
                packet_hash,
                raw_line,
            ),
        )
        return cur.rowcount > 0


def read_loop(config: Config, conn: psycopg2.extensions.connection) -> None:
    while True:
        try:
            with serial.Serial(config.serial_port, config.baud_rate, timeout=2) as ser:
                logging.info(
                    "Connected to %s at %s baud; writing to %s:%s/%s",
                    config.serial_port,
                    config.baud_rate,
                    config.db_host,
                    config.db_port,
                    config.db_name,
                )

                while True:
                    raw_bytes = ser.readline()
                    if not raw_bytes:
                        continue

                    line = raw_bytes.decode(errors="ignore").strip()
                    if not line:
                        continue

                    packet = parse_data_line(line)
                    if not packet:
                        logging.debug("Ignoring non-DATA line: %s", line)
                        continue

                    try:
                        inserted = store_packet(conn, packet, line)
                    except psycopg2.Error as exc:
                        logging.error("Database error '%s'; attempting reconnect", exc)
                        conn.close()
                        time.sleep(config.reconnect_delay_seconds)
                        conn = connect_db(config)
                        ensure_schema(conn)
                        try:
                            inserted = store_packet(conn, packet, line)
                        except psycopg2.Error as exc2:
                            logging.error("Failed to store after reconnect: '%s'", exc2)
                            continue
                    if inserted:
                        logging.info(
                            "Stored packet from node %s (fw %s) ts=%s temp=%.2fC humidity=%.1f%% co2=%s ppm",
                            packet["node_id"],
                            packet["version"],
                            packet["device_ts"],
                            packet["temp_c"],
                            packet["humidity_pct"],
                            packet["co2_ppm"],
                        )
                    else:
                        logging.debug("Duplicate packet ignored (hash match) for ts=%s raw=%s", packet["device_ts"], line)
        except serial.SerialException as exc:
            logging.warning(
                "Serial error '%s'; retrying in %.1f s", exc, config.reconnect_delay_seconds
            )
            time.sleep(config.reconnect_delay_seconds)
        except KeyboardInterrupt:
            logging.info("Stopping listener (keyboard interrupt)")
            break


def main() -> None:
    init_logging()
    config = load_config()
    while True:
        try:
            conn = connect_db(config)
            ensure_schema(conn)
            break
        except psycopg2.Error as exc:
            logging.error(
                "Database connection failed '%s'; retrying in %.1f s",
                exc,
                config.reconnect_delay_seconds,
            )
            time.sleep(config.reconnect_delay_seconds)

    read_loop(config, conn)


if __name__ == "__main__":
    main()
