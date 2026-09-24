# 0009. Optional local MQTT broker

## Context

The host was only an MQTT client. Replacing Home Assistant as the LAN broker needs an onboard server that other clients can reach.

## Decision

SERVER TYPE `local` starts a lightweight MQTT 3.1.1 broker on the **host only**. It binds all interfaces (`0.0.0.0`) on the stored PORT so remote LAN clients can connect (AP or STA address). The host client connects to `127.0.0.1`.

CONNECT requires the stored USERNAME and PASSWORD only when **both** are non-empty. If either is empty, the broker accepts anonymous CONNECT. An open LAN can then publish device commands; that is accepted until the operator fills both fields.

While the broker is listening, host **LED4** stays on (boot/critical still wins). Remote or local MQTT connected keeps host RGB **green**.

`disable` starts neither broker nor client. `remote` is client-only to the stored SERVER.

## Consequences

Do not run the broker on the slave. Do not require a web-console session for MQTT. Empty credentials on a STA LAN expose the command topics to anyone who can reach the host IP.
