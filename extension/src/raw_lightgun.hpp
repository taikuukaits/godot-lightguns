// raw_lightgun.hpp
// Windows Raw Input support for multiple Ultimarc AimTrak light guns in Godot 4.
// Based on the RawMouse GDExtension by henrylalonde (Godot Forum), adapted for
// absolute-position light guns with per-device button handling and stable
// player assignment via USB VID/PID.

#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/string.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace godot {

class RawLightgun : public RefCounted {
	GDCLASS(RawLightgun, RefCounted)

public:
	static const int MAX_GUNS = 4;
	static const int NUM_BUTTONS = 3; // 0 = left (trigger), 1 = right, 2 = middle

private:
	struct Gun {
		HANDLE handle = INVALID_HANDLE_VALUE;
		Vector2 aim = Vector2(0.5f, 0.5f); // normalized 0..1 over the screen
		bool has_aim = false;              // becomes true after first absolute packet
		bool virtual_desktop = false;      // absolute coords span the whole virtual desktop
		bool buttons[NUM_BUTTONS] = { false, false, false };
		int press_counts[NUM_BUTTONS] = { 0, 0, 0 }; // presses since last poll
		String device_name;
	};

	Gun guns[MAX_GUNS];
	HWND hwnd = nullptr;
	bool restrict_to_ultimarc = true;

	int find_or_assign(HANDLE device);

protected:
	static void _bind_methods();

public:
	WNDPROC original_wndproc = nullptr;

	RawLightgun();
	~RawLightgun();

	// Called from the window procedure on WM_INPUT.
	void handle_input(LPARAM lParam);

	// --- Script API ---
	bool is_gun_connected(int gun) const;
	Vector2 get_aim_position(int gun) const; // normalized 0..1 over the screen
	bool has_aim(int gun) const;
	bool is_virtual_desktop(int gun) const;
	bool is_button_pressed(int gun, int button) const;
	int take_button_presses(int gun, int button); // returns & clears press count
	String get_device_name(int gun) const;

	// If true (default), only devices with Ultimarc's USB vendor ID (D209) are
	// assigned to gun slots. Set false to test with two ordinary mice.
	void set_restrict_to_ultimarc(bool value);
	bool get_restrict_to_ultimarc() const;
};

} // namespace godot
