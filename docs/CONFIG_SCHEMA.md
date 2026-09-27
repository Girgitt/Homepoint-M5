# Homepoint-M5 application configuration

Homepoint-M5 stores normal application configuration in `/config.json` on
LittleFS. The configured station hostname belongs to this application
configuration. Wi-Fi credentials and web-administration credentials remain in
the versioned bootstrap record so an invalid application configuration does not
remove recovery/web access. Older bootstrap records may still contain a
hostname, but it is used only as a one-way migration/recovery fallback.

The current application schema is **schema version 2**.

## Schema selection and backward compatibility

Configuration parsing follows these rules:

1. `schemaVersion: 2` and a `tiles` array -> parse the native schema-v2 tile
   format.
2. `schemaVersion: 2`, no `tiles`, but a `scenes` array -> accept the old
   Homepoint scene format as a compatibility fallback.
3. Missing `schemaVersion`, or `schemaVersion: 1` -> parse the legacy `scenes`
   format.
4. A schema version newer than the firmware supports -> reject the
   configuration instead of guessing its meaning.

Legacy configuration is **not rewritten automatically**. Both formats are
normalized into the same runtime tile model after parsing.

A new configuration should use:

```json
{
  "schemaVersion": 2,
  "hostname": "homepoint-m5",
  "tiles": []
}
```

## Complete schema-v2 example

```json
{
  "schemaVersion": 2,
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
| `schemaVersion` | integer | Missing means legacy/implicit v1. Current native schema is `2`. |
| `mqttbroker` | string | Empty disables MQTT. |
| `mqttusername` | string | Empty means connect without MQTT username/password. |
| `mqttpasswd` | string | Used when `mqttusername` is non-empty. |
| `timezone` | string | POSIX TZ string passed to Arduino `configTzTime()`. |
| `tiles` | array | Native schema-v2 dashboard entries. |
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

Schema v2 currently supports `switch`, `sensor`, and `scene` tiles.

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

In schema v2, `type` identifies the tile or scene item itself, so the old
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
- a schema-v2 scene must contain **at least two items**;
- current scene-item types are `switch` and `sensor`;
- a one-device dashboard entry should instead be represented directly as a
  `switch` or `sensor` tile;
- mixed switch/sensor scene contents are accepted;
- scene items use the same switch/sensor field rules documented above.

The scene short-press action currently affects switch members only. Sensor
members are display/state entries and are not published to.

## Tile order and paging

The order of objects in `tiles` is the dashboard order. There is no separate
dashboard-layout registry in schema v2.

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

For a legacy scene containing exactly one device, create a direct tile. For
example, convert:

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
  "name": "Desk Lamp",
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
