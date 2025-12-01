# Script Runner test script
cmd("LORA EXAMPLE")
wait_check("LORA STATUS BOOL == 'FALSE'", 5)
