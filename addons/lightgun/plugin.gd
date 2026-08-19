@tool
extends EditorPlugin
## Registers the LightgunService autoload. The RawLightgun class itself comes
## from lightgun.gdextension, which Godot loads on its own — enabling the
## plugin is only about the autoload.

const AUTOLOAD_NAME := "LightgunService"
const AUTOLOAD_PATH := "res://addons/lightgun/lightgun_service.gd"


func _enable_plugin() -> void:
	add_autoload_singleton(AUTOLOAD_NAME, AUTOLOAD_PATH)


func _disable_plugin() -> void:
	remove_autoload_singleton(AUTOLOAD_NAME)
