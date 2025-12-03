# Default Scope Contents

This scope is mounted by the COSMOS containers to hold installed plugins and any runtime edits made through the UI. It currently contains the FireLord LoRa plugin instance:

- `openc3-cosmos-lora/`: prebuilt `openc3-cosmos-lora-1.0.0.gem` plus `plugin_instance.json` that maps target `LORA` to `host.docker.internal:2950`.

Treat this folder as configuration-controlled storage. After running COSMOS, additional files may appear here; commit only the intentional plugin changes.
