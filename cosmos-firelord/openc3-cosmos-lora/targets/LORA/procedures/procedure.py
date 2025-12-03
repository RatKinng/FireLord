# FireLord nodes are TX-only. Use this script to confirm SAMPLE telemetry is flowing.
wait_check("LORA SAMPLE VERSION == '1'", 30)
wait_check("LORA SAMPLE STATUS_FLAGS != '0'", 30)
