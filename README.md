## Open Home Security GW (fingerprint fork)

Gateway code for the OHS board v 2.0.x custom board with STM32F4x7.
Builds against [vysocan/ChibiOS_21.11.x](https://github.com/vysocan/ChibiOS_21.11.x) —
a ChibiOS/RT 21.11.x tree with the OHS board files already added, not vanilla
ChibiOS-RT.

This is a fork of [vysocan/OHS_2-gateway](https://github.com/vysocan/OHS_2-gateway)
(see https://openhomesecurity.blogspot.com/ for the original project), extended
with fingerprint-reader support and a number of protocol fixes. Current version:
**2.0.1**.

The companion fingerprint-reader node firmware lives at
[p2baron/OHS_2-STM32_node_1_0_RS485_Finger](https://github.com/p2baron/OHS_2-STM32_node_1_0_RS485_Finger) —
this gateway is the counterpart that drives it; you need both.

### Differences from upstream

**Fingerprint (R503) support** — the headline addition. The gateway keeps a
20-slot fingerprint directory (`conf.finger[]`) plus a 32KB RAM/flash mirror of
every enrolled template (`fpBuf`), and talks to one or more RS485 fingerprint
nodes over a small `'F'+<sub-command>` protocol:

- **Enroll / Delete** a fingerprint on a chosen node from the Fingerprints page,
  with live sync — a newly enrolled template is automatically pushed out to every
  other fingerprint node on the bus, so any node can recognize any enrolled finger.
- **Pull FP** — re-fetch a template straight from a node's physical sensor and
  store it on the gateway (backup/restore of the gateway's own record; does not
  re-distribute to peers).
- **Resync FP / Flush FP** (Nodes page) — push all of the gateway's known
  templates to one node, or wipe a node's sensor entirely.
- **Sensor status** — each node unsolicitedly reports its sensor's own template
  count, shown as a live "is this slot actually present on the sensor" checkbox,
  independent of the gateway's own bookkeeping.
- **Panic fingers** — any enrolled finger can be flagged "panic"; using it arms/
  disarms exactly as normal, but also fires a dedicated, independently
  configurable alert (SMS/Page/Email/MQTT) via the Alerts page.
- Fingerprint events get proper logging/display: who armed/disarmed by finger,
  enroll/delete/flush/resync events, and fingerprint auth linked to the right
  contact.
- The triggering zone's name and exit/entry countdown timing are pushed to
  fingerprint nodes so their displays can show live arming/alarm status.

**Protocol robustness fixes** — several RS485 message handlers used to send a
blocking, ACKed reply straight from inside the receive handler. That risks
stalling the handling thread and dropping whatever the gateway sends next if
two messages land close together. These are now deferred to each node's own
idle-tick, never sent inline:

- the fingerprint ID-table query reply (fires automatically after every node
  registration),
- the sensor-template-count report,
- the Pull FP template reply.

**Other fixes in this fork**: NFC key misrouting and arm-rejected log-prefix
bugs, `config.bin` upload and a watchdog crash during slow downloads, and an
alert-type bitmask ordering bug (a new alert type must always be *appended*,
never inserted, since `conf.alert[]` is a persisted bitmask keyed by position
in the alert-type table).

### Upstream features retained

Everything from the original project — zones, groups, keys, timers, triggers,
scripts, SMS/modem integration, MQTT, HTTP admin UI — is unchanged and still
present; fingerprint support is additive.
