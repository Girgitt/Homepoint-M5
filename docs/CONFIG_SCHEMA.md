# Homepoint-M5 application configuration

Homepoint-M5 stores normal application configuration in `/config.json` on
LittleFS. The configured station hostname belongs to this application
configuration. Wi-Fi credentials and web-administration credentials remain in
the versioned bootstrap record so an invalid application configuration does not
remove recovery/web access. Older bootstrap records may still contain a
hostname, but it is used only as a one-way migration/recovery fallback.

The current application schema is **schema version 3**. Schema version 2 remains readable and is upgraded only by an explicit user action.

## Dashboard editor API

The dashboard editor intentionally works with a whole-dashboard document rather
than exposing firmware-side per-tile CRUD operations:

```text
GET  /api/dashboard
POST /api/dashboard/validate
PUT  /api/dashboard
GET  /api/layouts
GET  /api/layout?file=layout_<name>.json
POST /api/layout/validate?file=...&name=...
PUT  /api/layout?file=...&name=...
PUT  /api/dashboard/source
POST /api/dashboard/upgrade
```

`GET` returns a normalized editable representation plus modest capability and
source metadata. `POST /api/dashboard/validate` strictly validates a complete
browser draft and returns the normalized representation without persisting it.
`PUT` validates the same document, merges only its dashboard into the existing
application configuration, saves through the normal atomic/last-good path, and
queues a runtime reload on the main application task.

Example envelope:

```json
{
  "schemaVersion": 3,
  "dashboard": {
    "tiles": []
  },
  "capabilities": {
    "tileTypes": ["switch", "sensor", "scene"],
    "sceneItemTypes": ["switch", "sensor"],
    "sensorTypes": ["singleValue", "combinedValues"],
    "maxVisibleTiles": 6,
    "minSceneItems": 2
  },
  "source": {
    "configuration": "active",
    "legacy": false,
    "layout": "inline"
  },
  "warnings": []
}
```

Clients may send the complete GET response back after editing; `capabilities`,
`source`, and `warnings` are informational and are ignored on validation/save.
Unrelated `/config.json` fields such as MQTT, hostname, UI and hardware settings
are preserved, including unknown future top-level fields. The API does not
expose live MQTT operations, so selecting or editing a dashboard item cannot
accidentally control equipment.

Editor validation is intentionally stricter than the legacy boot-time
compatibility loader. Present fields must have the documented JSON type,
`sensorType` must be exactly `singleValue` or `combinedValues`, tile IDs must be
strings, and scene/item type names must be recognized. This prevents browser
mistakes such as `"jsondata": "true"` or a misspelled sensor type from being
silently converted to defaults.

`source.configuration` reports whether a response represents the active
`config.json`, recovered `config.lastgood.json`, or a validation-only `draft`.
Recovery and legacy conversion are also listed in `warnings`, so the editor can
make those states visible instead of silently hiding them.

Reading a legacy configuration does not rewrite it. Legacy scenes with two or
more members can be represented losslessly as schema-v2 scenes and are exposed
that way with a migration warning; the migration becomes persistent only after
a successful PUT. Legacy scenes with zero or one member are **not** collapsed
to a direct tile because that would discard either scene-level or device-level
name/icon information during an ordinary read/save. In that case GET returns a
conflict error, but the explicit **Upgrade schema** action remains available.
Upgrade intentionally converts a one-device legacy scene to the corresponding
direct switch/sensor tile. The visible scene-level `name` and `icon` are kept
for the direct tile, while MQTT/value/sensor fields come from the sole legacy
device. A zero-device legacy scene still cannot be migrated and must be
corrected in the raw configuration first.

The typed layout endpoints deliberately own managed `layout_*.json` files. The
generic file editor can display those files but cannot overwrite/delete them;
this keeps filename validation, layout-schema validation, atomic persistence and
last-good handling on one code path. `/api/layouts` reports both the currently
active source and all discovered managed layouts. Saving an inactive layout does
not reload the runtime. `PUT /api/dashboard/source` performs the explicit
activation/copy-inline operation, while `POST /api/dashboard/upgrade` performs
the explicit legacy/schema-v2-to-v3 externalization.

The dashboard endpoints use distinct status classes so the browser can separate
user errors from device/storage failures:

- `400` — malformed or schema-invalid editor draft;
- `409` — the stored configuration cannot be represented/merged safely (for
  example a lossy legacy scene shape);
- `413` — request exceeds the 128 KiB editor-body limit;
- `500` — persistence/activation I/O failure;
- `503` — LittleFS is unavailable;
- `507` — request/JSON/output allocation could not be satisfied.

## Schema selection and backward compatibility

Configuration parsing follows these rules:

1. `schemaVersion: 3` and `tiles` as an array -> parse an inline dashboard.
2. `schemaVersion: 3` and `tiles` as a string -> resolve the named managed
   `layout_*.json` file and use its tile definition.
3. `schemaVersion: 2` and a `tiles` array -> parse the schema-v2 inline tile
   format without rewriting it.
4. `schemaVersion: 2`, no `tiles`, but a `scenes` array -> accept the old
   Homepoint scene format as a compatibility fallback.
5. Missing `schemaVersion`, or `schemaVersion: 1` -> parse legacy `scenes`.
6. A schema version newer than the firmware supports -> reject the
   configuration instead of guessing its meaning.

Normal dashboard Save does not silently promote schema v2 to v3. The web editor
provides an explicit **Upgrade schema** action that materializes the current
effective dashboard as a layout file and then changes `config.json` to reference
it. The same action is available for the legacy implicit-v1 `scenes` format;
one-device legacy scenes are deliberately converted to direct tiles as part of
that explicit migration.

A new configuration should use schema v3, either inline:

```json
{
  "schemaVersion": 3,
  "hostname": "homepoint-m5",
  "tiles": []
}
```

or with an external layout reference:

```json
{
  "schemaVersion": 3,
  "hostname": "homepoint-m5",
  "tiles": "layout_ground_floor.json"
}
```

### External layout documents

Managed layout filenames must match `layout_[A-Za-z0-9_-]+.json`. The file
contains a separate required human-readable `name`, so display names are not
constrained by filesystem naming rules. The name is trimmed, must not be empty,
and is limited to 96 characters:

```json
{
  "kind": "homepoint-layout",
  "schemaVersion": 1,
  "name": "Ground Floor / Office",
  "tiles": [
    {
      "id": "desk-lamp",
      "type": "switch",
      "name": "Desk Lamp",
      "getTopic": "lights/desk/state",
      "setTopic": "lights/desk/set"
    }
  ]
}
```

Each layout uses its own atomic staging and last-good recovery files. These
managed transaction files are hidden from the generic file editor. A valid
`config.json` may therefore recover an invalid active layout independently from
`config.lastgood.json`.

The dashboard editor distinguishes the file being edited from the source active
on the device. Saving an inactive layout does not reload runtime state;
activation is explicit. **Use inline copy** copies a selected layout into the
schema-v3 inline `tiles` array while keeping the layout file.

## Complete schema-v3 inline example

```json
{
  "schemaVersion": 3,
  "hostname": "homepoint-m5",

  "mqttbroker": "mqtt://192.168.2.1:1883",
  "mqttusername": "homepoint",
  "mqttpasswd": "secret",
  "timezone": "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00",

  "ui": {
    "longPressMs": 600
  },

  "screenSaverMinutes": 10,
  "screenSaverPowerSaveEnabled": true,
  "powerFrom5vRailNotUsb": true,
  "screenRotationAngle": 1,
  "displayColorInverted": false,

  "tiles": [
    {
      "id": "desk-lamp",
      "type": "switch",
      "name": "Desk Lamp",
      "setTopic": "lights/desk/set",
      "getTopic": "lights/desk/state",
      "onValue": "true",
      "offValue": "false",
      "icon": "lamp"
    },
    {
      "id": "bedroom-temperature",
      "type": "sensor",
      "name": "Bedroom",
      "getTopic": "bedroom/esptemp",
      "jsondata": true,
      "sensorType": "singleValue",
      "firstKey": "temperature",
      "firstIcon": "temperature_small",
      "icon": "bedroom"
    },
    {
      "id": "living-room",
      "type": "scene",
      "name": "Living Room",
      "icon": "livingroom",
      "items": [
        {
          "type": "switch",
          "name": "Ceiling",
          "setTopic": "lights/living/ceiling/set",
          "getTopic": "lights/living/ceiling/state",
          "onValue": "true",
          "offValue": "false"
        },
        {
          "type": "switch",
          "name": "Floor Lamp",
          "setTopic": "lights/living/floor/set",
          "getTopic": "lights/living/floor/state",
          "onValue": "true",
          "offValue": "false"
        }
      ]
    }
  ]
}
```

`data/config.example.json` contains the repository copy of this style of
configuration.

## Top-level fields

| Field | Type | Default / behavior |
| --- | --- | --- |
| `schemaVersion` | integer | Missing means legacy/implicit v1. Current native schema is `3`; schema v2 remains readable. |
| `mqttbroker` | string | Empty disables MQTT. |
| `mqttusername` | string | Empty means connect without MQTT username/password. |
| `mqttpasswd` | string | Used when `mqttusername` is non-empty. |
| `timezone` | string | POSIX TZ string passed to Arduino `configTzTime()`. |
| `tiles` | array or string | Schema v3: inline dashboard array or `layout_*.json` reference. Schema v2: inline array only. |
| `scenes` | array | Legacy compatibility input. |
| `ui` | object | UI behavior settings. |
| `screenSaverMinutes` | integer | Default `10`. |
| `screenSaverPowerSaveEnabled` | boolean | Default `true`. |
| `powerFrom5vRailNotUsb` | boolean | Default `true`; handled through M5Unified, not direct PMIC register access. |
| `screenRotationAngle` | integer | Default `1`. |
| `displayColorInverted` | boolean | Default `false`. |
| `powerSaveMHz` | integer | Legacy-compatible field, default `80`. |
| `ledPinPullup` | boolean | Legacy-compatible field, default `false`. |
| `touchXAxisInverted` | boolean | Legacy-compatible field, default `false`. |
| `touchYAxisInverted` | boolean | Legacy-compatible field, default `true`. |

### Timezone syntax

`timezone` uses the POSIX `TZ` format expected by `configTzTime()`, not an
ordinary signed UTC offset. POSIX offset signs are reversed: `CET-1` means
UTC+1 and `CEST-2` means UTC+2. For Poland / Central Europe the example:

```text
CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00
```

selects CET (UTC+1) in winter and CEST (UTC+2) in summer, with the DST
transition rules encoded in the same string.

## MQTT broker syntax

The current MQTT parser accepts:

```text
mqtt://192.168.2.1:1883
tcp://192.168.2.1:1883
192.168.2.1:1883
192.168.2.1
```

Port `1883` is used when no port is specified. URI schemes other than `mqtt://`
and `tcp://` are rejected; MQTT TLS is not implemented yet.

## UI settings

### `ui.longPressMs`

Example:

```json
"ui": {
  "longPressMs": 600
}
```

The accepted range is **200 through 5000 milliseconds**. The default is
`600` ms.

The current debug/compatibility UI uses the generic short/long-press classifier
as follows:

- direct `switch` tile: short press toggles, long press opens detail;
- direct `sensor` tile: short or long press opens detail;
- `scene` tile: short press toggles its switch members, long press opens scene
  detail.

These mappings are consumers of the generic tile-action mechanism and are not
intended to limit future tile types such as dimmers.

## Tile types

The schema-v2/v3 native tile grammar supports `switch`, `sensor`, and `scene` tiles.

### Direct switch tile

Use a direct switch tile when the dashboard entry represents one controllable
switch. Do not wrap a single switch in a one-item scene.

```json
{
  "id": "desk-lamp",
  "type": "switch",
  "name": "Desk Lamp",
  "setTopic": "lights/desk/set",
  "getTopic": "lights/desk/state",
  "onValue": "ON",
  "offValue": "OFF",
  "icon": "lamp"
}
```

Validation and defaults:

- `getTopic` is required;
- `setTopic` is required;
- `name` defaults to `"Unnamed"`;
- `onValue` defaults to `"true"`;
- `offValue` defaults to `"false"`;
- `id` is currently optional, but a non-empty ID should be unique and is
  strongly recommended for stable configuration identity;
- duplicate non-empty tile IDs are rejected.

The switch state changes when an MQTT payload on `getTopic` exactly matches
`onValue` or `offValue`.

### Direct sensor tile

A raw MQTT value sensor can be defined as:

```json
{
  "id": "outside-temperature",
  "type": "sensor",
  "name": "Outside",
  "getTopic": "sensors/outside/temperature",
  "jsondata": false,
  "sensorType": "singleValue",
  "firstIcon": "temperature"
}
```

With `jsondata: false`, the complete MQTT payload becomes the first displayed
value.

A JSON sensor can be defined as:

```json
{
  "id": "boiler-temperature",
  "type": "sensor",
  "name": "Boiler",
  "getTopic": "tele/boiler/SENSOR",
  "jsondata": true,
  "sensorType": "singleValue",
  "firstKey": "Temperature",
  "firstIcon": "temperature"
}
```

`firstKey` is searched recursively through JSON objects and arrays, preserving
the old Homepoint behavior where a configured key may be nested below the
payload root.

A two-value JSON sensor uses `combinedValues`:

```json
{
  "id": "room-climate",
  "type": "sensor",
  "name": "Room Climate",
  "getTopic": "tele/room/SENSOR",
  "jsondata": true,
  "sensorType": "combinedValues",
  "firstKey": "Temperature",
  "secondKey": "Humidity",
  "firstIcon": "temperature",
  "secondIcon": "humidity"
}
```

Validation and defaults:

- `getTopic` is required;
- `sensorType` defaults to `singleValue`;
- `sensorType: "combinedValues"` selects the two-value representation;
- with `jsondata: true`, `firstKey` is required;
- with `jsondata: true` and `combinedValues`, `secondKey` is also required;
- with `jsondata: false`, the whole payload is stored in the first value;
- `name` defaults to `"Unnamed"`.

In the native schema-v2/v3 tile grammar, `type` identifies the tile or scene item itself, so the old
sensor value-shape field named `type` has been renamed to `sensorType`.

### Scene tile

A scene is a real multi-item dashboard group:

```json
{
  "id": "living-room",
  "type": "scene",
  "name": "Living Room",
  "items": [
    {
      "type": "switch",
      "name": "Ceiling",
      "setTopic": "lights/living/ceiling/set",
      "getTopic": "lights/living/ceiling/state",
      "onValue": "ON",
      "offValue": "OFF"
    },
    {
      "type": "sensor",
      "name": "Temperature",
      "getTopic": "sensors/living/temperature",
      "jsondata": false,
      "sensorType": "singleValue"
    }
  ]
}
```

Rules:

- `items` is required and must be an array;
- a native schema-v2/v3 scene must contain **at least two items**;
- current scene-item types are `switch` and `sensor`;
- a one-device dashboard entry should instead be represented directly as a
  `switch` or `sensor` tile;
- mixed switch/sensor scene contents are accepted;
- scene items use the same switch/sensor field rules documented above.

The scene short-press action currently affects switch members only. Sensor
members are display/state entries and are not published to.

## Tile order and paging

The order of objects in `tiles` is the dashboard order. Schema v2 stores that
array only inline. Schema v3 can keep it inline or resolve it from one managed
external layout file; the runtime model is identical after resolution.

The compatibility UI currently shows six dashboard tiles per page. Its lower
navigation bar is hidden when all tiles fit on one page, allowing the tiles to
use the reclaimed display area.

## Legacy Homepoint `scenes` format

Existing Homepoint configurations remain readable. A typical old switch scene
looks like:

```json
{
  "scenes": [
    {
      "name": "Living Room",
      "type": "Switch",
      "devices": [
        {
          "name": "Ceiling",
          "setTopic": "lights/living/ceiling/set",
          "getTopic": "lights/living/ceiling/state",
          "onValue": "ON",
          "offValue": "OFF"
        }
      ]
    }
  ]
}
```

Legacy scene types accepted by the current compatibility parser are:

- `"Switch"`;
- `"Light"`;
- `"Sensor"`.

For a legacy sensor scene, each object in `devices` keeps the historical
sensor `type` field:

```json
{
  "name": "Climate",
  "type": "Sensor",
  "devices": [
    {
      "name": "Room",
      "type": "combinedValues",
      "jsondata": true,
      "getTopic": "tele/room/SENSOR",
      "firstKey": "Temperature",
      "secondKey": "Humidity"
    }
  ]
}
```

A legacy one-device scene remains a scene at runtime for compatibility. It is
not silently converted into a direct schema-v2 device tile.

## Migrating legacy configuration to schema v2

Migration is intentionally explicit rather than automatic.

For a legacy scene containing exactly one device, create a direct tile. Preserve
the scene-level `name` and `icon`, because those values identify what the user
actually saw on the dashboard; copy the MQTT/value/sensor fields from the sole
device. For example, convert:

```json
{
  "name": "Desk",
  "type": "Switch",
  "devices": [
    {
      "name": "Desk Lamp",
      "setTopic": "lights/desk/set",
      "getTopic": "lights/desk/state",
      "onValue": "ON",
      "offValue": "OFF"
    }
  ]
}
```

into:

```json
{
  "id": "desk-lamp",
  "type": "switch",
  "name": "Desk",
  "setTopic": "lights/desk/set",
  "getTopic": "lights/desk/state",
  "onValue": "ON",
  "offValue": "OFF"
}
```

For a true multi-device group, convert the old scene into a schema-v2
`type: "scene"` tile and rename `devices` to `items`, giving every item its own
lower-case `type`.

For sensors, rename the old per-device sensor value-shape `type` field to
`sensorType`, because schema v2 reserves `type` for the tile/item kind.

## Validation and atomic save behavior

The web configuration editor validates JSON before activating it. A successful
save is staged through `config.new.json`. If the current active configuration
is itself valid, it is copied to `config.lastgood.json` before the replacement
is activated.

If active `/config.json` cannot be loaded during boot but
`config.lastgood.json` is valid, Homepoint-M5 loads the last-good copy instead.

`hostname` is limited to 32 characters because it is passed to the
Arduino-ESP32 DHCP client. The hostname must be installed before the Wi-Fi
station interface starts, so changing it in the web configuration editor causes
one controlled reboot after the new `config.json` has been committed. Ordinary
configuration changes continue to use the normal explicit Apply action.

For upgrades from earlier Homepoint-M5 builds, a missing/empty `hostname` is
filled once from the legacy hostname stored in the bootstrap record (or from
`homepoint-m5` if no legacy value exists). This migration writes only
`config.json`; it never rewrites EEPROM and never requests a boot-time reboot.
Therefore a bad EEPROM record or failed persistence cannot create a
hostname-reconciliation reboot loop.

The bootstrap record remains responsible for Wi-Fi credentials and web
credentials. Its historical hostname field is retained only so existing schema
v1/v2 records remain readable.

## Hybrid dashboard editor

The embedded administration page consumes the dashboard API as one browser-owned
draft. Editing is intentionally separated from operation of real equipment:

- the **Dashboard structure** pane owns ordering, add/delete operations, and scene
  membership;
- the **Inspector** edits the selected tile or scene item;
- the **Simulated Core2 screen** mirrors the 3x2 user dashboard layout and can be
  clicked to select a top-level tile, but it never publishes MQTT commands;
- **Validate** posts the complete draft to `/api/dashboard/validate` without
  persistence;
- **Save** PUTs the complete draft to `/api/dashboard` and accepts the normalized
  response as the new saved baseline;
- **Revert** restores the last GET/successful-save representation held by the
  browser.

For schema-v3 layout sources, the selector at the top of the editor identifies
what is being edited independently from what the device is currently using.
`+ New layout` copies the current draft into a new named `layout_*.json` file,
**Set as active** changes `config.json` to schema v3 and references the selected
persisted layout, and **Use inline copy** copies the selected persisted layout
back into a schema-v3 inline `config.json`. **Upgrade schema** is offered for an older inline configuration
and creates a named external layout before changing `config.json` to schema v3.
A layout recovered from its last-good backup must be saved (repairing its active
file) before it can be newly activated.

The editor reads `tileTypes`, `sceneItemTypes`, `sensorTypes`,
`maxVisibleTiles`, and `minSceneItems` from the API capability envelope. Device
source warnings, including last-good recovery and lossless legacy conversion,
are displayed above the editor rather than hidden.

This editor remains configuration-only after the schema-v3/layout-source slice.
Runtime/live-state visualization and an explicit operate mode are separate later
slices; no browser-to-MQTT control path is introduced here.
