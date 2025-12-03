# Plugins Volume (FireLord)

This directory is bind-mounted into COSMOS so plugin gems and instance metadata persist across container restarts. For FireLord we keep the LoRa plugin preloaded here so a fresh `./openc3.sh run` comes up with the `LORA` target available.

- `DEFAULT/`: default COSMOS scope. Runtime writes here, so expect timestamps or cache files after COSMOS runs.  
- `DEFAULT/openc3-cosmos-lora/`: packaged `openc3-cosmos-lora` gem and its `plugin_instance.json`, which points to `host.docker.internal:2950` for LoRa traffic.

If you rebuild the plugin, replace the gem in `DEFAULT/openc3-cosmos-lora/` and reload via the COSMOS Admin UI or `./openc3.sh cli load ...` (see `cosmos-firelord/README.md`). Keep `README.upstream.md` for the original OpenC3 guidance.
