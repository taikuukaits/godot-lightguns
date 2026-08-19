extends Node2D
## Debug scene: one colored crosshair per gun, shot markers on trigger, and an
## overlay showing what each slot is bound to.

const PLAYER_COLORS := [
	Color(0.2, 0.8, 1.0),
	Color(1.0, 0.4, 0.3),
	Color(0.5, 1.0, 0.4),
	Color(1.0, 0.9, 0.3),
]
const SHOWN_PLAYERS := 2
const MARKER_LIFETIME := 1.0

var _markers: Array[Dictionary] = []

@onready var _overlay: Label = $Overlay


func _ready() -> void:
	LightgunService.shot_fired.connect(_on_shot_fired)
	LightgunService.hide_cursor()


func _process(delta: float) -> void:
	for marker in _markers:
		marker.age += delta
	_markers = _markers.filter(func(m): return m.age < MARKER_LIFETIME)

	_update_overlay()
	queue_redraw()


func _input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel"):
		LightgunService.show_cursor()


func _on_shot_fired(player: int, position: Vector2) -> void:
	_markers.append({"player": player, "position": position, "age": 0.0})


func _draw() -> void:
	for marker in _markers:
		var t: float = marker.age / MARKER_LIFETIME
		var color: Color = PLAYER_COLORS[marker.player % PLAYER_COLORS.size()]
		color.a = 1.0 - t
		draw_circle(marker.position, 6.0 + 30.0 * t, color, false, 3.0)

	for player in SHOWN_PLAYERS:
		if not (LightgunService.is_gun_connected(player) and LightgunService.has_aim(player)):
			continue
		_draw_crosshair(LightgunService.aim(player), PLAYER_COLORS[player])


func _draw_crosshair(pos: Vector2, color: Color) -> void:
	const R := 24.0
	const GAP := 6.0
	draw_arc(pos, R, 0.0, TAU, 48, color, 2.0)
	draw_line(pos + Vector2(-R, 0), pos + Vector2(-GAP, 0), color, 2.0)
	draw_line(pos + Vector2(GAP, 0), pos + Vector2(R, 0), color, 2.0)
	draw_line(pos + Vector2(0, -R), pos + Vector2(0, -GAP), color, 2.0)
	draw_line(pos + Vector2(0, GAP), pos + Vector2(0, R), color, 2.0)


func _update_overlay() -> void:
	var lines: Array[String] = ["Lightgun debug — Esc restores the cursor"]
	for player in LightgunService.MAX_PLAYERS:
		var connected := LightgunService.is_gun_connected(player)
		var line := "P%d: %s" % [player + 1, "connected" if connected else "—"]
		if connected:
			var aim := LightgunService.aim_normalized(player)
			line += "  aim(%.3f, %.3f)%s  trigger:%s" % [
				aim.x, aim.y,
				"" if LightgunService.has_aim(player) else " (no absolute data yet)",
				"HELD" if LightgunService.is_trigger_held(player) else "-",
			]
			line += "\n    " + LightgunService.get_device_name(player)
		lines.append(line)
	_overlay.text = "\n".join(lines)
