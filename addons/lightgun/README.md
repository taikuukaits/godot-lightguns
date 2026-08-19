# Lightgun addon

Per-device aim and trigger input for Ultimarc AimTrak light guns, for Godot 4.6
on Windows x86_64. Ships precompiled — no C++ toolchain needed to use it.

## Install

1. Copy the whole `addons/lightgun/` folder into your project's `addons/`.
2. Restart the editor. GDExtensions only load at startup, so the `RawLightgun`
   class will not appear until you do.
3. **Project → Project Settings → Plugins** → enable **Lightgun**. That adds the
   `LightgunService` autoload.

Nothing else to wire up — `LightgunService` is global from then on.

## Use

```gdscript
func _ready() -> void:
	LightgunService.shot_fired.connect(_on_shot_fired)
	LightgunService.hide_cursor()

func _on_shot_fired(player: int, position: Vector2) -> void:
	var params := PhysicsPointQueryParameters2D.new()
	params.position = position
	for hit in get_world_2d().direct_space_state.intersect_point(params):
		hit.collider.take_damage(player)

func _process(_delta: float) -> void:
	for player in 2:
		crosshairs[player].visible = LightgunService.is_gun_connected(player) \
				and LightgunService.has_aim(player)
		if crosshairs[player].visible:
			crosshairs[player].position = LightgunService.aim(player)
```

The `godot-lightguns` repository this addon is built from ships a test scene
(`scenes/lightgun_test.tscn`) with crosshairs, shot markers, and a device
overlay — a useful reference if you want to confirm your guns are seen before
touching your own scenes.

## API

| Member | Purpose |
| --- | --- |
| `shot_fired(player, position)` | Trigger pull, position in viewport coords |
| `gun_connected(player)` / `gun_disconnected(player)` | Hotplug |
| `aim(player) -> Vector2` | Live aim in viewport coords |
| `aim_normalized(player) -> Vector2` | Raw 0..1, unconverted |
| `is_gun_connected(player)` / `has_aim(player)` | Gate drawing on these |
| `is_trigger_held(player)` | Auto-fire, charge shots |
| `is_button_pressed(player, button)` | 0 = trigger, 1 = right, 2 = middle |
| `get_device_name(player)` | USB device path, for debug overlays |
| `hide_cursor()` / `show_cursor()` | Both guns move the shared OS cursor |

`shot_fired` may emit **twice in one frame** for the same player — deliberate, so
fast trigger pulls are never dropped. Don't assume one shot per frame.

## Player slots

Give each gun a unique **Device ID** in the AimTrak Configuration Utility and
calibrate it there. The ID lands in the USB PID and pins the player slot:

| Device ID | USB PID | Player |
| --- | --- | --- |
| 1 | `PID_1601` | 0 |
| 2 | `PID_1602` | 1 |
| 3 | `PID_1603` | 2 |
| 4 | `PID_1604` | 3 |

Stable across reboots, USB ports, firing order, and mid-session replugs. The
desktop mouse is ignored and never claims a slot.

## Requirements and limits

- Windows x86_64 only. The extension is Win32 Raw Input; there is no macOS or
  Linux binary and it will simply not load there.
- Godot 4.6 or newer (`compatibility_minimum = "4.6"`).
- Run **fullscreen on the monitor the guns were calibrated against**, or aim
  lands offset.
- Only one `RawLightgun` instance may exist — it owns the window-proc chain.
  `LightgunService` owns it; never construct another.
- Exporting a game: Godot bundles the DLLs from `.gdextension` automatically.

## Testing without guns

Edit `lightgun_service.gd` and set `allow_any_mouse = true` (autoload `@export`s
aren't Inspector-editable), then plug in two ordinary USB mice. Crosshairs won't
move — ordinary mice are relative devices and never send absolute positions —
but you can confirm device separation and per-device triggers.

## Rebuilding

Source and build script live outside the addon, in `extension/`. See the
repository README.
