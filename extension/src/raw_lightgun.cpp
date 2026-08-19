// raw_lightgun.cpp

#include "raw_lightgun.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/core/print_string.hpp>

#include <wchar.h>

using namespace godot;

// Single active instance. Using a file-scope pointer instead of GWLP_USERDATA
// keeps us from clobbering anything the engine might store on the window.
static RawLightgun *g_lightgun_instance = nullptr;

static LRESULT CALLBACK lightgun_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	RawLightgun *lg = g_lightgun_instance;
	if (lg && uMsg == WM_INPUT) {
		lg->handle_input(lParam);
	}
	if (lg && lg->original_wndproc) {
		return CallWindowProcW(lg->original_wndproc, hwnd, uMsg, wParam, lParam);
	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// Returns the gun slot for this device handle, assigning a new slot if needed.
// AimTrak guns report a device path containing VID_D209 (Ultimarc) and
// PID_1601..PID_1604, where the last digit is the Device ID you set in the
// AimTrak configuration utility. That gives us a stable player order no matter
// which gun fires first or which USB port it's in.
// A handle belonging to an unplugged device stops resolving, which is how we
// tell a live slot from one holding a corpse.
static bool is_handle_live(HANDLE h) {
	UINT size = 0;
	return GetRawInputDeviceInfoW(h, RIDI_DEVICENAME, nullptr, &size) != (UINT)-1;
}

int RawLightgun::find_or_assign(HANDLE device) {
	for (int i = 0; i < MAX_GUNS; i++) {
		if (guns[i].handle == device) {
			return i;
		}
	}

	// Unknown device: read its interface path.
	UINT size = 0;
	GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &size);
	if (size == 0 || size > 1024) {
		return -1;
	}
	wchar_t name[1025] = { 0 };
	if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name, &size) == (UINT)-1) {
		return -1;
	}

	const bool is_ultimarc = (wcsstr(name, L"VID_D209") != nullptr || wcsstr(name, L"vid_d209") != nullptr);
	if (restrict_to_ultimarc && !is_ultimarc) {
		return -1; // Ignore the desktop mouse, trackpad, etc.
	}

	int slot = -1;

	// Replug: same device path, new handle. Take the slot back so the player
	// keeps their gun instead of the stale handle holding it hostage.
	const String path = String(name);
	for (int i = 0; i < MAX_GUNS; i++) {
		if (guns[i].handle != INVALID_HANDLE_VALUE && guns[i].device_name == path) {
			guns[i].handle = device;
			return i;
		}
	}

	// Preferred: stable slot from the AimTrak PID (PID_1601 -> gun 0, etc).
	const wchar_t *pid = wcsstr(name, L"PID_160");
	if (!pid) {
		pid = wcsstr(name, L"pid_160");
	}
	if (pid) {
		wchar_t digit = pid[7];
		if (digit >= L'1' && digit <= L'0' + MAX_GUNS) {
			int wanted = (int)(digit - L'1');
			if (guns[wanted].handle == INVALID_HANDLE_VALUE || !is_handle_live(guns[wanted].handle)) {
				slot = wanted;
			}
		}
	}

	// Fallback: first free slot, in order of first input received.
	if (slot < 0) {
		for (int i = 0; i < MAX_GUNS; i++) {
			if (guns[i].handle == INVALID_HANDLE_VALUE || !is_handle_live(guns[i].handle)) {
				slot = i;
				break;
			}
		}
	}

	if (slot >= 0) {
		guns[slot].handle = device;
		guns[slot].device_name = path;
	}
	return slot;
}

void RawLightgun::handle_input(LPARAM lParam) {
	RAWINPUT raw;
	UINT dwSize = sizeof(RAWINPUT);

	if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
				&raw, &dwSize, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
		return;
	}
	if (raw.header.dwType != RIM_TYPEMOUSE) {
		return;
	}

	int idx = find_or_assign(raw.header.hDevice);
	if (idx < 0) {
		return;
	}
	Gun &g = guns[idx];

	// AimTraks report absolute coordinates in the 0..65535 range mapped over
	// the screen (or the whole virtual desktop if MOUSE_VIRTUAL_DESKTOP is set).
	if (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
		g.aim.x = static_cast<float>(raw.data.mouse.lLastX) / 65535.0f;
		g.aim.y = static_cast<float>(raw.data.mouse.lLastY) / 65535.0f;
		g.has_aim = true;
		g.virtual_desktop = (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
	}

	const USHORT flags = raw.data.mouse.usButtonFlags;
	const USHORT down_flags[NUM_BUTTONS] = {
		RI_MOUSE_LEFT_BUTTON_DOWN,
		RI_MOUSE_RIGHT_BUTTON_DOWN,
		RI_MOUSE_MIDDLE_BUTTON_DOWN,
	};
	const USHORT up_flags[NUM_BUTTONS] = {
		RI_MOUSE_LEFT_BUTTON_UP,
		RI_MOUSE_RIGHT_BUTTON_UP,
		RI_MOUSE_MIDDLE_BUTTON_UP,
	};
	for (int b = 0; b < NUM_BUTTONS; b++) {
		if (flags & down_flags[b]) {
			g.buttons[b] = true;
			g.press_counts[b] += 1;
		}
		if (flags & up_flags[b]) {
			g.buttons[b] = false;
		}
	}
}

void RawLightgun::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_gun_connected", "gun"), &RawLightgun::is_gun_connected);
	ClassDB::bind_method(D_METHOD("get_aim_position", "gun"), &RawLightgun::get_aim_position);
	ClassDB::bind_method(D_METHOD("has_aim", "gun"), &RawLightgun::has_aim);
	ClassDB::bind_method(D_METHOD("is_virtual_desktop", "gun"), &RawLightgun::is_virtual_desktop);
	ClassDB::bind_method(D_METHOD("is_button_pressed", "gun", "button"), &RawLightgun::is_button_pressed);
	ClassDB::bind_method(D_METHOD("take_button_presses", "gun", "button"), &RawLightgun::take_button_presses);
	ClassDB::bind_method(D_METHOD("get_device_name", "gun"), &RawLightgun::get_device_name);
	ClassDB::bind_method(D_METHOD("set_restrict_to_ultimarc", "value"), &RawLightgun::set_restrict_to_ultimarc);
	ClassDB::bind_method(D_METHOD("get_restrict_to_ultimarc"), &RawLightgun::get_restrict_to_ultimarc);
}

RawLightgun::RawLightgun() {
	if (g_lightgun_instance != nullptr) {
		print_line("RawLightgun: an instance already exists; only one is supported.");
		return;
	}

	// Register for raw mouse input. NOTE: no RIDEV_NOLEGACY here — we still
	// want Godot's normal (merged) mouse events for menus and the editor.
	RAWINPUTDEVICE rid;
	rid.usUsagePage = 0x01; // Generic desktop controls
	rid.usUsage = 0x02;     // Mouse
	rid.dwFlags = 0;
	rid.hwndTarget = nullptr; // Follows keyboard focus.

	if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == FALSE) {
		print_line("RawLightgun: raw input registration failed.");
		return;
	}

	hwnd = reinterpret_cast<HWND>(DisplayServer::get_singleton()->window_get_native_handle(
			DisplayServer::HandleType::WINDOW_HANDLE));
	if (!hwnd) {
		print_line("RawLightgun: failed to retrieve HWND.");
		return;
	}

	g_lightgun_instance = this;
	original_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
			hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lightgun_wndproc)));
}

RawLightgun::~RawLightgun() {
	if (g_lightgun_instance == this) {
		if (hwnd && original_wndproc) {
			SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wndproc));
		}
		g_lightgun_instance = nullptr;
	}
}

bool RawLightgun::is_gun_connected(int gun) const {
	if (gun < 0 || gun >= MAX_GUNS) {
		return false;
	}
	return guns[gun].handle != INVALID_HANDLE_VALUE;
}

Vector2 RawLightgun::get_aim_position(int gun) const {
	if (gun < 0 || gun >= MAX_GUNS) {
		return Vector2(0.5f, 0.5f);
	}
	return guns[gun].aim;
}

bool RawLightgun::has_aim(int gun) const {
	if (gun < 0 || gun >= MAX_GUNS) {
		return false;
	}
	return guns[gun].has_aim;
}

bool RawLightgun::is_virtual_desktop(int gun) const {
	if (gun < 0 || gun >= MAX_GUNS) {
		return false;
	}
	return guns[gun].virtual_desktop;
}

bool RawLightgun::is_button_pressed(int gun, int button) const {
	if (gun < 0 || gun >= MAX_GUNS || button < 0 || button >= NUM_BUTTONS) {
		return false;
	}
	return guns[gun].buttons[button];
}

int RawLightgun::take_button_presses(int gun, int button) {
	if (gun < 0 || gun >= MAX_GUNS || button < 0 || button >= NUM_BUTTONS) {
		return 0;
	}
	int n = guns[gun].press_counts[button];
	guns[gun].press_counts[button] = 0;
	return n;
}

String RawLightgun::get_device_name(int gun) const {
	if (gun < 0 || gun >= MAX_GUNS) {
		return String();
	}
	return guns[gun].device_name;
}

void RawLightgun::set_restrict_to_ultimarc(bool value) {
	restrict_to_ultimarc = value;
}

bool RawLightgun::get_restrict_to_ultimarc() const {
	return restrict_to_ultimarc;
}
