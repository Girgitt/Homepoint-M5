# Bootstrap migration scope

The first Homepoint-M5 milestone is a behavioural/platform migration, not a UI
redesign.

## Preserve

- Core2 touchscreen appliance behaviour.
- Home screen containing ordered dashboard tiles.
- Legacy Homepoint scene configurations through the compatibility parser.
- Scene-level switch commands.
- Device-level switch commands.
- Sensor values from raw or JSON MQTT payloads.
- Recursive lookup of a configured JSON sensor key, matching the old ability
  to find values such as Tasmota `Power` below the top level.
- Wi-Fi/MQTT/time status.
- Screen timeout and touch wake.
- First-use configuration AP.
- Browser-based configuration/file management.
- JSON application configuration with schema-v2 `tiles` and legacy `scenes` compatibility.
- OTA firmware update.
- Failsafe/recovery access.

## Change now

- ESP-IDF application + embedded Arduino -> PlatformIO Arduino application.
- M5Core2/AXP192-specific code -> M5Unified/M5GFX.
- SPIFFS -> LittleFS.
- Wi-Fi/web bootstrap credentials -> versioned EEPROM-backed record.
- Hostname -> authoritative `/config.json` setting applied before station Wi-Fi starts; the historical EEPROM hostname is migration-only.
- MQTT startup dependency -> optional service with backoff.
- Invalid JSON -> degraded state; no reboot loop.
- Main admin/setup pages -> embedded firmware pages so the recovery UI does
  not depend on a healthy filesystem.
- Save config -> validate + `config.new.json` + `config.lastgood.json`.

## Preserve as compatibility keys, but do not implement by poking PMIC rails

The parser retains older hardware settings. M5Unified should own hardware
revision-specific details. `powerFrom5vRailNotUsb` is mapped to
`M5.Power.setExtOutput()` rather than direct AXP192 register operations.

## Configuration model after bootstrap

Schema version 2 makes the dashboard explicit through a top-level `tiles`
array. Direct switch/sensor devices are tiles themselves; only genuine
multi-item groups use `type: "scene"`. Legacy `scenes` input remains supported
and is normalized into the same runtime tile model. See `CONFIG_SCHEMA.md` for
the full contract and migration examples.

## Defer

- LVGL.
- Final component/view registry.
- Plot/detail-screen architecture.
- Dimmer-specific full-screen editor.
- Icons/theme migration.
- SDL desktop UI simulator.
- MQTT TLS and richer connection profiles.

Those should be specified only after this baseline is proven on real Core2
hardware.
