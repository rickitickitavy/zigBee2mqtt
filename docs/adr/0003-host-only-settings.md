# 0003. Host settings vs slave entity stores

## Context

The slave loses RAM on reboot. Radio parameters must come from the host after `SLAVE_READY`. Flashing host LittleFS wipes host files, so entity lists that live only on the host are lost even when the slave filesystem is intact.

## Decision

The host is the only store for product settings: Wi-Fi, MQTT, Zigbee radio parameters, and hardware. After `SLAVE_READY`, the host pushes radio settings (`SET_SETTINGS`) before normal Zigbee work.

Registered devices and console users are stored on the slave LittleFS. The host keeps a RAM cache pulled after settings. CRUD is one create/update or one delete over SPI. The host MUST NOT send a full-table replace. The slave MUST NOT persist an empty user or device list, including when the host cache is empty after a LittleFS flash.

A join report does not register a device; the operator Save on the host does.

## Consequences

Wiping host LittleFS/EEPROM loses Wi-Fi/MQTT/radio settings, not the slave user or device tables. Login after a host filesystem flash waits for the user pull. The slave seeds `admin`/`admin` only when its own user file is empty.
