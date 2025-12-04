"""Simple web viewer for FireLord packets with live polling and clear-all control."""

import os
from typing import List, Dict

from flask import Flask, jsonify, render_template_string, request

from .logger import Config, connect_db, ensure_schema

app = Flask(__name__)


def get_config() -> Config:
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


def fetch_packets(limit: int = 200) -> List[Dict]:
    cfg = get_config()
    conn = connect_db(cfg)
    ensure_schema(conn)
    with conn.cursor() as cur:
        cur.execute(
            """
            SELECT id, recorded_at, node_id, version, device_ts, temp_c, humidity_pct,
                   co2_ppm, pressure_pa, voc_raw, smoke_raw, flags, raw_line
            FROM packets
            ORDER BY id DESC
            LIMIT %s;
            """,
            (limit,),
        )
        rows = cur.fetchall()
    conn.close()
    columns = [
        "id",
        "recorded_at",
        "node_id",
        "version",
        "device_ts",
        "temp_c",
        "humidity_pct",
        "co2_ppm",
        "pressure_pa",
        "voc_raw",
        "smoke_raw",
        "flags",
        "raw_line",
    ]
    return [dict(zip(columns, row)) for row in rows]


def clear_packets() -> None:
    cfg = get_config()
    conn = connect_db(cfg)
    with conn.cursor() as cur:
        cur.execute("TRUNCATE TABLE packets;")
    conn.close()


INDEX_TEMPLATE = """
<!doctype html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>FireLord Packet Viewer</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; background: #0f172a; color: #e2e8f0; }
    header { padding: 12px 16px; background: #111827; border-bottom: 1px solid #1f2937; display: flex; justify-content: space-between; align-items: center; }
    .wrapper { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; padding: 12px; height: calc(100vh - 56px); box-sizing: border-box; }
    .panel { background: #111827; border: 1px solid #1f2937; border-radius: 8px; display: flex; flex-direction: column; overflow: hidden; }
    .panel h2 { margin: 0; padding: 10px 12px; border-bottom: 1px solid #1f2937; font-size: 16px; }
    .list { flex: 1; overflow: auto; padding: 10px 12px; }
    .item { padding: 8px; margin-bottom: 8px; background: #0b1224; border: 1px solid #1f2937; border-radius: 6px; }
    .item small { display: block; color: #94a3b8; }
    button { background: #2563eb; color: #fff; border: none; border-radius: 6px; padding: 8px 12px; cursor: pointer; }
    button.danger { background: #b91c1c; }
    button:disabled { opacity: 0.6; cursor: not-allowed; }
    table { width: 100%; border-collapse: collapse; }
    th, td { text-align: left; padding: 6px; border-bottom: 1px solid #1f2937; }
    th { background: #0b1224; position: sticky; top: 0; }
  </style>
</head>
<body>
  <header>
    <div>
      <strong>FireLord Packet Viewer</strong>
      <span id="status" style="margin-left:8px;color:#94a3b8;">Polling…</span>
    </div>
    <div>
      <button class="danger" onclick="clearAll()">Clear all data</button>
    </div>
  </header>
  <div class="wrapper">
    <div class="panel">
      <h2>Recent Packets (raw line)</h2>
      <div class="list" id="rawList"></div>
    </div>
    <div class="panel">
      <h2>Decoded Fields</h2>
      <div class="list" style="padding:0;">
        <table id="decodedTable">
          <thead>
            <tr>
              <th>ID</th><th>Recorded</th><th>Node</th><th>TS</th><th>Temp C</th><th>Humidity</th><th>CO2</th><th>Flags</th>
            </tr>
          </thead>
          <tbody></tbody>
        </table>
      </div>
    </div>
  </div>

  <script>
    const statusEl = document.getElementById('status');
    const rawList = document.getElementById('rawList');
    const tableBody = document.querySelector('#decodedTable tbody');

    async function loadPackets() {
      try {
        statusEl.textContent = 'Polling…';
        const res = await fetch('/api/packets');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();
        renderRaw(data);
        renderTable(data);
        statusEl.textContent = 'Updated ' + new Date().toLocaleTimeString();
      } catch (err) {
        statusEl.textContent = 'Error: ' + err;
      }
    }

    function renderRaw(data) {
      rawList.innerHTML = '';
      data.forEach(p => {
        const div = document.createElement('div');
        div.className = 'item';
        div.innerHTML = '<div>' + (p.raw_line || '') + '</div>' +
          '<small>#' + p.id + ' • ' + p.recorded_at + '</small>';
        rawList.appendChild(div);
      });
    }

    function renderTable(data) {
      tableBody.innerHTML = '';
      data.forEach(p => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
          <td>${p.id}</td>
          <td>${p.recorded_at}</td>
          <td>${p.node_id}</td>
          <td>${p.device_ts}</td>
          <td>${p.temp_c?.toFixed(2)}</td>
          <td>${p.humidity_pct?.toFixed(1)}</td>
          <td>${p.co2_ppm}</td>
          <td>${p.flags}</td>
        `;
        tableBody.appendChild(tr);
      });
    }

    async function clearAll() {
      const confirmClear = confirm('This will DELETE ALL packet data. Continue?');
      if (!confirmClear) return;
      statusEl.textContent = 'Clearing…';
      const res = await fetch('/api/clear', { method: 'POST' });
      if (!res.ok) {
        statusEl.textContent = 'Clear failed: HTTP ' + res.status;
        return;
      }
      await loadPackets();
    }

    loadPackets();
    setInterval(loadPackets, 3000);
  </script>
</body>
</html>
"""


@app.route("/")
def index():
    return render_template_string(INDEX_TEMPLATE)


@app.route("/api/packets")
def api_packets():
    packets = fetch_packets(limit=200)
    return jsonify(packets)


@app.route("/api/clear", methods=["POST"])
def api_clear():
    clear_packets()
    return jsonify({"status": "ok"})


def main() -> None:
    port = int(os.getenv("VIEWER_PORT", "8000"))
    app.run(host="0.0.0.0", port=port)


if __name__ == "__main__":
    main()
