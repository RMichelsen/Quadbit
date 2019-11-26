#pragma once

#include <Windows.h>
#include <cstdint>

inline constexpr RAWINPUTDEVICE RAW_INPUT_DEVICES[2] = {
	RAWINPUTDEVICE{0x01, 0x02, 0, nullptr},  // Mouse
	RAWINPUTDEVICE{0x01, 0x06, 0, nullptr}   // Keyboard
};

namespace Quadbit {
	struct KeyboardControlKeys {
		bool del;
		bool backspace;
		bool enter;
		bool tab;
		bool left;
		bool right;
		bool up;
		bool down;
	};
	struct MouseButtonStatus {
		bool left;
		bool right;
		bool middle;
		bool xbutton1;
		bool xbutton2;
	};
	struct MouseDrag {
		bool active;
		uint32_t duration;
		POINT cachedPos;
	};
	struct MouseDragStatus {
		MouseDrag left;
		MouseDrag right;
	};
	struct MouseDelta {
		float x;
		float y;
	};
}