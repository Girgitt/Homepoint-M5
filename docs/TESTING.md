# Testing and verification

Homepoint-M5 uses two automated verification layers and a small manual hardware
smoke test. The layers deliberately test different failure classes; a native
host test does not replace a Core2 build or a real-device check.

## VS Code / PlatformIO tasks

The PlatformIO bottom-bar flask button runs plain `platformio test`. Because the
project keeps `m5stack-core2` as `default_envs`, that generic button selects the
Core2 environment. The Core2 environment explicitly ignores `test_native`, so the
host-only suite cannot accidentally be linked or uploaded as firmware. The generic
flask button still does not run the native regression suite; use `Test Native`.

Use the PlatformIO Project Tasks explorer instead:

```text
PROJECT TASKS
├── native
│   └── Custom
│       ├── Test Native
│       └── Verify All
└── m5stack-core2
    └── Custom
        ├── Test Native
        └── Verify All
```

`Test Native` runs `pio test -e native`. `Verify All` runs the native suite and
then builds the Core2 firmware. Refresh the PlatformIO Task Explorer after
changing `platformio.ini` if the custom tasks are not visible immediately.

## Native regression tests

Run:

```bash
scripts/test-native.sh
```

or directly:

```bash
pio test -e native
```

The native environment executes on the development host and does not require an
M5Stack Core2. It tests deterministic application logic, including:

- CRC16 bootstrap integrity helper;
- haptic pulse timing;
- short/long-press classification;
- wake-gesture consumption and screen-power state;
- status-bar time/IP cycling;
- semantic `UiChange` addressing;
- tile behavior resolution;
- strict dashboard-editor request parsing and normalization;
- dashboard scene cardinality, duplicate IDs and sensor-type/type validation;
- lossless legacy-dashboard handling and last-good recovery metadata;
- dashboard replacement preserving unrelated and unknown configuration fields;
- `UiController` navigation, command generation, haptic intent, detail paging,
  display-mode behavior, configuration replacement and screen-power behavior.

`UiController.cpp` and the pure `DashboardCodec.cpp` are compiled into the native
test binary. The native environment pins ArduinoJson as a host dependency.
`test/test_native/stubs/Arduino.h` provides only the small `String` surface
required by the application model. This stub is not visible to firmware builds.

## Core2 firmware build

Run:

```bash
pio run -e m5stack-core2
```

This cross-compiles and links the real firmware using the pinned Arduino-ESP32,
M5Unified, M5GFX, ESPAsyncWebServer, ArduinoJson and MQTT dependencies. It
catches target API, compile, link, partition and firmware-size failures, but it
does not exercise physical hardware.

## Full automated verification

Run:

```bash
scripts/verify.sh
```

This runs the native regression suite first and then the complete Core2 firmware
build. It is the normal pre-commit/pre-merge verification entry point.

## Hardware smoke test

Some behavior necessarily requires a physical Core2 and network environment.
After changes that touch the relevant subsystem, verify at least:

- boot and LittleFS mount;
- Wi-Fi association, DHCP hostname and reconnect;
- MQTT receive and command publish;
- short/long touch and haptic feedback;
- screensaver wake-gesture consumption;
- MQTT/time updates while the physical display is asleep;
- web screenshot capture while awake and asleep;
- file edit/upload/download and configuration recovery;
- OTA update and reboot into the new firmware.

## Hybrid dashboard editor checks

The native suite also carries a lightweight contract check for the embedded
admin page. It verifies that the hierarchy, simulated Core2 preview and
inspector are present and that the page uses the whole-dashboard GET/validate/PUT
API rather than per-tile or live-control endpoints.

For changes to the embedded JavaScript, additionally extract the admin `<script>`
body and run `node --check` when Node.js is available. Browser-level behavior to
exercise manually is: load an existing dashboard, select from hierarchy and
preview, edit fields without losing focus, reorder tiles/items, add/delete,
validate without saving, revert, save, and confirm that no editor action controls
MQTT equipment.
