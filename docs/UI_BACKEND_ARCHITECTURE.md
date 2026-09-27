# UI backend/controller boundary

Homepoint-M5 separates semantic UI behavior from the rendering framework.
The current implementation still uses M5GFX, but it now sits behind the same
boundary intended for a future LVGL backend.

## Flow

```text
model/network state
      |
      v
   UiChange --------------------------+
      |                               |
      v                               v
 UiBackend                       renderer update

raw backend input
      |
      v
   UiIntent
      |
      v
 UiController
      |
      +-- navigation/view state
      +-- haptic feedback request
      +-- UiCommand
             |
             v
            App
             |
             v
        MqttManager
```

`UiController` owns renderer-independent interaction state:

- Home vs tile-detail navigation;
- selected tile and page indexes;
- USER/DEBUG display mode;
- transient UI message state;
- screen-power/wake-guard state;
- tile short/long-press meaning;
- footer navigation meaning;
- semantic haptic cues;
- creation of commands such as setting a tile or tile item on/off.

The controller does not draw pixels and does not publish MQTT messages.

`UiBackend` is the rendering/input contract used by the application. The
current `CompatUi` implementation is the M5GFX backend. It owns:

- M5GFX drawing and dirty-region mapping;
- M5Unified raw touch hit testing;
- conversion of touch/gesture events into `UiIntent` values;
- physical display brightness;
- haptic motor/timing implementation;
- mapping `UiChange` values to backend-specific redraws.

MQTT commands are executed by `App`, not by the renderer. This keeps a future
LVGL backend from needing any MQTT-specific behavior.

## Future LVGL backend

A future backend can implement `UiBackend`, translate LVGL events into the
same `UiIntent` values, and consume the existing `UiChange` stream. Only one
backend needs to be active at a time. Renderer selection should initially be a
boot-time configuration choice; hot switching renderers is intentionally out
of scope.

The existing display screenshot service remains backend-independent because it
captures the actual LCD contents after rendering.
