extends Node
## Owns the single RawLightgun instance and exposes per-player aim and trigger
## events. Autoloaded, so gameplay code never touches the extension directly.

signal shot_fired(player: int, position: Vector2)
signal gun_connected(player: int)
signal gun_disconnected(player: int)

const MAX_PLAYERS := 4
const TRIGGER_BUTTON := 0

## Set true to test with ordinary USB mice instead of AimTraks. Ordinary mice
## are relative devices, so aim will not update — only device separation and
## per-device buttons can be verified this way.
@export var allow_any_mouse := false

var _guns: RawLightgun
var _connected := [false, false, false, false]


func _ready() -> void:
	process_priority = -100
	_guns = RawLightgun.new()
	_guns.set_restrict_to_ultimarc(not allow_any_mouse)


func _process(_delta: float) -> void:
	if _guns == null:
		return

	for player in MAX_PLAYERS:
		var now := _guns.is_gun_connected(player)
		if now != _connected[player]:
			_connected[player] = now
			if now:
				gun_connected.emit(player)
			else:
				gun_disconnected.emit(player)

		# take_button_presses() drains a counter, so two pulls inside one frame
		# both fire rather than collapsing into one.
		var shots: int = _guns.take_button_presses(player, TRIGGER_BUTTON)
		if shots > 0 and now:
			var pos := aim(player)
			for _i in shots:
				shot_fired.emit(player, pos)


## Aim position in viewport coordinates. Returns Vector2.ZERO if the gun has
## not reported a position yet — check has_aim() first.
func aim(player: int) -> Vector2:
	if _guns == null or not _guns.has_aim(player):
		return Vector2.ZERO
	return _aim_to_local(_guns.get_aim_position(player))


## Normalized 0..1 aim, straight from the device.
func aim_normalized(player: int) -> Vector2:
	if _guns == null:
		return Vector2.ZERO
	return _guns.get_aim_position(player)


func is_gun_connected(player: int) -> bool:
	return _guns != null and _guns.is_gun_connected(player)


func has_aim(player: int) -> bool:
	return _guns != null and _guns.has_aim(player)


func get_device_name(player: int) -> String:
	if _guns == null:
		return ""
	return _guns.get_device_name(player)


func is_trigger_held(player: int) -> bool:
	return _guns != null and _guns.is_button_pressed(player, TRIGGER_BUTTON)


func is_button_pressed(player: int, button: int) -> bool:
	return _guns != null and _guns.is_button_pressed(player, button)


func hide_cursor() -> void:
	Input.mouse_mode = Input.MOUSE_MODE_HIDDEN


func show_cursor() -> void:
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


## Raw Input absolute coordinates span either the current screen or the whole
## virtual desktop (MOUSE_VIRTUAL_DESKTOP). Resolve against whichever rect the
## device reported, then convert screen -> window -> viewport.
func _aim_to_local(aim_normalized_pos: Vector2) -> Vector2:
	var origin: Vector2
	var size: Vector2

	if _guns.is_virtual_desktop(0) or _guns.is_virtual_desktop(1):
		var rect := _virtual_desktop_rect()
		origin = rect.position
		size = rect.size
	else:
		var screen := DisplayServer.window_get_current_screen()
		origin = Vector2(DisplayServer.screen_get_position(screen))
		size = Vector2(DisplayServer.screen_get_size(screen))

	var screen_pos := origin + aim_normalized_pos * size
	var window_pos := screen_pos - Vector2(DisplayServer.window_get_position())
	var window_size := Vector2(DisplayServer.window_get_size())
	if window_size.x == 0 or window_size.y == 0:
		return window_pos
	return window_pos * Vector2(get_viewport().get_visible_rect().size) / window_size


func _virtual_desktop_rect() -> Rect2:
	var rect := Rect2(Vector2(DisplayServer.screen_get_position(0)), Vector2(DisplayServer.screen_get_size(0)))
	for i in range(1, DisplayServer.get_screen_count()):
		rect = rect.merge(Rect2(
			Vector2(DisplayServer.screen_get_position(i)),
			Vector2(DisplayServer.screen_get_size(i))))
	return rect
