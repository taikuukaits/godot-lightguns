# godot-lightguns

Dual Ultimarc AimTrak light gun support for Godot 4.6 on Windows, via a custom
GDExtension (`RawLightgun`) that reads the Win32 Raw Input API directly.

Windows merges every mouse into one system cursor, so per-gun aim is only
available through `WM_INPUT` + `raw.header.hDevice`. The extension subclasses
the Godot window proc, keeps per-device absolute aim and button state, and
assigns stable player slots from the AimTrak USB PID.

## Layout

| Path | What |
| --- | --- |
| `extension/src/` | The `RawLightgun` GDExtension C++ sources |
| `extension/SConstruct` | Build script (godot-cpp master, `api_version=4.6`) |
| `bin/` | Built DLLs, loaded by `lightgun.gdextension` |
| `autoload/lightgun_service.gd` | `LightgunService` autoload — the API gameplay code uses |
| `scenes/lightgun_test.tscn` | Debug scene: crosshairs, shot markers, device overlay |

## Player assignment

Set a unique **Device ID** per gun in the AimTrak Configuration Utility. The ID
lands in the USB PID, and the extension maps it to a fixed slot:

| AimTrak Device ID | USB PID | Player |
| --- | --- | --- |
| 1 | `PID_1601` | 0 |
| 2 | `PID_1602` | 1 |
| 3 | `PID_1603` | 2 |
| 4 | `PID_1604` | 3 |

Assignment survives reboots, USB port changes, and firing order. Calibrate each
gun in the same utility, and run the game fullscreen on the calibrated monitor.

## Using it

```gdscript
LightgunService.shot_fired.connect(_on_shot_fired)

func _on_shot_fired(player: int, position: Vector2) -> void:
    var params := PhysicsPointQueryParameters2D.new()
    params.position = position
    for hit in get_world_2d().direct_space_state.intersect_point(params):
        hit.collider.take_damage(player)
```

`LightgunService.aim(player)` gives the live crosshair position in viewport
coordinates; `is_gun_connected(player)` and `has_aim(player)` gate drawing.

Triggers are counted, not sampled — two pulls inside a single frame emit
`shot_fired` twice.

## Building the extension

Requires MSVC (C++17), Python 3, SCons.

```sh
cd extension
git clone --depth 1 https://github.com/godotengine/godot-cpp
python -m SCons platform=windows target=template_debug
python -m SCons platform=windows target=template_release
```

godot-cpp has no `4.6` branch — master ships `extension_api-4-6.json` and picks
the target at build time, which is what `SConstruct` passes as `api_version`.

## Testing without hardware

Set `allow_any_mouse = true` on the `LightgunService` autoload and plug in two
ordinary USB mice. Ordinary mice are relative devices and never send
`MOUSE_MOVE_ABSOLUTE`, so crosshairs stay put — what this verifies is device
separation and per-device trigger events.

## Notes

- Both guns keep moving the shared OS cursor. That is unavoidable; the cursor is
  hidden during gameplay (Esc restores it in the test scene).
- `RIDEV_NOLEGACY` is deliberately not set, so Godot's ordinary merged mouse
  input still drives menus and the editor.
- Only one `RawLightgun` may exist — it owns the window-proc chain. The autoload
  owns it; do not construct another.
- Absolute coordinates span the current screen, or the whole virtual desktop when
  the device sets `MOUSE_VIRTUAL_DESKTOP`. `_aim_to_local()` handles both.
