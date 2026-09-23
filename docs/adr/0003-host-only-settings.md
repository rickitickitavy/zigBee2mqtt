# 0003. Host is the only settings store

## Context

The slave loses RAM on reboot. Product settings must survive independently of the radio chip.

## Decision

The host is the only store for product settings: Wi-Fi, MQTT, Zigbee radio parameters, registered devices, and console users. The slave MUST NOT persist that map or recover a local settings file.

After `SLAVE_READY`, the host pushes radio settings (`SET_SETTINGS`) before normal Zigbee work. A join report does not register a device; the operator Save on the host does.

## Consequences

Wiping host LittleFS/EEPROM loses devices and users even if the slave is intact. The slave always waits for a fresh settings push after reset.
