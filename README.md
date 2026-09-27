# Homepoint-M5

A clean M5Stack-focused continuation of Homepoint.

This repository starts by preserving the useful Homepoint behaviour while
removing the old ESP-IDF + embedded-Arduino build arrangement. The initial
target is M5Stack Core2; the hardware layer uses M5Unified so later support for
other M5 touch devices does not require PMIC-specific application code.

## Bootstrap scope

This initial repository intentionally **does not use LVGL yet**. It provides a
small M5GFX compatibility UI so that the platform migration can be validated
before the UI architecture changes.

Implemented in the bootstrap:

- PlatformIO + Arduino-only application framework.
- M5Unified/M5GFX hardware access.
- Versioned EEPROM-backed bootstrap settings with:
  - magic value;
  - schema version;
  - stored payload size;
  - CRC16-IBM;
  - explicit uninitialized/corrupt/unsupported-version detection;
  - a place for future schema migration code.
- First-start access point:
  - SSID `HomePoint-Config`;
  - address `192.168.99.1`;
  - Wi-Fi scan and selection;
  - web administrator credentials.
- Recovery AP after prolonged station connection failure.
- LittleFS application filesystem.
- Schema-v2 JSON application configuration using typed dashboard `tiles`, with
  backward-compatible parsing of old Homepoint MQTT/scenes/sensors keys.
- Automatic one-time import of old `wifi`, `password`, `hostname`, `login` and
  `webpass` keys from `/config.json` when the EEPROM bootstrap record is still
  uninitialized. The station hostname itself is authoritative in `config.json`;
  the EEPROM copy is retained only as a migration/recovery fallback.
- Embedded recovery/setup web pages: a damaged or missing LittleFS image does
  not remove the configuration channel.
- Authenticated web UI with:
  - bounded two-pane LittleFS file manager with recursive directory listing, search, visible independent scrollbars, and a draggable height control for the list/editor;
  - inline JPEG/PNG/GIF/BMP/SVG preview and explicit download;
  - in-browser editing/creation of JSON and common text files with unsaved-change warnings;
  - atomic file replacement for browser edits/uploads, including upload destination paths;
  - JSON syntax validation for generic JSON edits/uploads and full application-schema
    validation plus last-good backup for `config.json`; `config.lastgood.json` is
    exposed read-only and direct upload replacement of `config.json` is refused;
  - on-demand capture and browser preview/download of the actual M5 display,
    with completion-aware 2/5/10-second refresh scheduling;
  - file upload/delete;
  - configuration reload;
  - reboot;
  - OTA firmware upload;
  - clear-bootstrap-settings action.
- MQTT is optional. Missing or unreachable MQTT leaves the UI and web
  configuration usable.
- MQTT reconnect is rate-limited instead of being tied directly to every Wi-Fi
  event.
- Basic scene/switch/sensor compatibility UI with direct M5GFX drawing.
- Screen saver uses display sleep/backlight control and leaves touch available
  for wake-up; it does not manipulate Core2 PMIC display/touch rails directly.

## Deliberate non-goals of this bootstrap

- Pixel-for-pixel reproduction of the old TFT_eSPI/TFT_eFEX GUI.
- The old JPEG icon renderer.
- LVGL.
- A final extensible `DeviceView`/screen registry API.
- TLS MQTT configuration.
- Automatic schema migration for future EEPROM versions (the dispatch point is
  present; version 1 is the first Homepoint-M5 schema).

Those belong in subsequent milestones after the behavioural baseline works on
real Core2 hardware.

## Build

```bash
pio run
```

Flash firmware:

```bash
pio run -t upload
```

Upload the initial LittleFS contents:

```bash
pio run -t uploadfs
```

Open serial monitor:

```bash
pio device monitor
```

Run the repository verification contract:

```bash
./scripts/verify.sh
```

The firmware can boot without `uploadfs`: it creates a minimal
`/config.json` on first successful LittleFS mount, and the setup/recovery page
is embedded in firmware.

## First boot

With no valid bootstrap settings record, Homepoint-M5 starts:

```text
SSID: HomePoint-Config
IP:   192.168.99.1
```

Open `http://192.168.99.1/`, select an SSID, enter its password and choose the
web UI administrator credentials. The device stores these values in the
versioned EEPROM record and reboots.

Wi-Fi/web credentials are deliberately separated from the application JSON.
This makes the recovery path independent from MQTT/scenes JSON validity.

## Application configuration

The application contract remains `/config.json`. New configurations use
**schema version 2**, whose top-level dashboard collection is `tiles`:

```json
{
  "schemaVersion": 2,
  "mqttbroker": "mqtt://192.168.2.1:1883",
  "ui": {
    "longPressMs": 600
  },
  "tiles": [
    {
      "id": "desk-lamp",
      "type": "switch",
      "name": "Desk Lamp",
      "setTopic": "lights/desk/set",
      "getTopic": "lights/desk/state",
      "onValue": "ON",
      "offValue": "OFF"
    }
  ]
}
```

Schema-v2 tile types currently include direct `switch` and `sensor` devices
and true multi-item `scene` groups. A one-device dashboard entry should be a
direct device tile rather than a one-item scene. The order of `tiles` is the
dashboard order.

Legacy Homepoint `scenes` configurations remain readable and are not rewritten
automatically. Schema v2 also accepts `scenes` as a compatibility fallback when
`tiles` is absent. Unsupported future schema versions are rejected instead of
being guessed.

`ui.longPressMs` controls the generic dashboard short/long-press classifier and
accepts values from 200 to 5000 milliseconds; the default is 600 ms.

See [`docs/CONFIG_SCHEMA.md`](docs/CONFIG_SCHEMA.md) for the complete schema,
validation rules, switch/sensor/scene examples, MQTT URI syntax and legacy
migration examples. The repository starter configuration is
[`data/config.example.json`](data/config.example.json).

## Persistence model

Bootstrap settings and application configuration have different failure
domains:

```text
EEPROM/NVS-backed record
  Wi-Fi SSID/password
  legacy hostname fallback only
  web login/password
  configured flag
        |
        +---- recovery and web access

LittleFS /config.json
  station hostname
  MQTT
  schema-v2 tiles / legacy scenes
  switch and sensor state definitions
  UI/hardware preferences
        |
        +---- normal Homepoint behaviour
```

A broken JSON configuration therefore does not make the setup/recovery channel
depend on MQTT or scene parsing.

The EEPROM record is deliberately a binary envelope (`magic`, schema version,
payload size, payload, CRC16-IBM). An erased record is treated as uninitialized;
a non-erased wrong magic or bad CRC is treated as corrupt; an unknown schema
version is reported separately. Future versions should add a `loadV<N>()` path
and explicit migration to the current in-memory `BootstrapSettings` rather than
reinterpreting an older structure.

## Original Homepoint reference

The behavioural migration baseline is the `Girgitt/Homepoint` fork at commit:

```text
e5016f54a20d0a92d73e4f8ba0969ecf7fdeafdd
```

Homepoint was originally created by Matthias Frick and distributed under the
MIT license.
