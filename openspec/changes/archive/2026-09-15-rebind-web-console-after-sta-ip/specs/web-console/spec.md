## ADDED Requirements

### Requirement: Console stays reachable after STA address refresh
When the host is in STA mode and the station obtains an IPv4 address after a disconnect, roam, or DHCP renew, the HTTP web console on port 80 MUST answer on that current STA address. ICMP reachability and MQTT client traffic MUST NOT be treated as proof that the console is already bound. The first HTTP bind after a successful boot join MUST remain the bind used until a later STA address event; that first bind MUST NOT be torn down immediately. USB CLI, MQTT, and Zigbee/SPI control MUST keep working while the console listen socket is rebound.

#### Scenario: Console answers after STA reconnect
- **WHEN** STA drops and later has an IPv4 address again
- **THEN** opening `http://<sta-ip>/` returns the web console HTML without requiring a device reboot

#### Scenario: Console answers after STA GOT_IP without a polled disconnect
- **WHEN** STA reports a new or renewed IPv4 address while ICMP and MQTT still work
- **THEN** opening `http://<sta-ip>/` returns the web console HTML

#### Scenario: First boot bind is not rebound
- **WHEN** the host has just bound the console after the boot STA join succeeded
- **THEN** the console remains that first listen and is not immediately closed and opened again
