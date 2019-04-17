#pragma once

namespace Quadbit::InputHandler {
	struct MouseButtonStatus {
		bool left;
		bool right;
		bool middle;
		bool xbutton1;
		bool xbutton2;
	};

	constexpr RAWINPUTDEVICE rawinputdevices[2]{
		{0x01, 0x02, 0, 0}, // Mouse
		{0x01, 0x06, 0, 0}	// Keyboard
	};

	// Mouse variables
	inline MouseButtonStatus mouseButtonStatus;
	inline std::array<int32_t, 2> deltaMouseMovement{ 0, 0 };
	inline POINT clientMousePos{ 0, 0 };

	inline bool leftMouseDragging = false;
	inline uint64_t leftClickFrameDuration = 0;
	inline POINT leftDragCachedPos{ 0, 0 };

	inline bool rightMouseDragging = false;
	inline uint64_t rightClickFrameDuration = 0;
	inline POINT rightDragCachedPos{ 0, 0 };

	// Keycodes for keyboard handling
	inline std::array<bool, 0xFF> keycodes{};


	inline bool Initialize() {
		return RegisterRawInputDevices(&rawinputdevices[0], 2, sizeof(RAWINPUTDEVICE));
	}

	inline void ProcessInput(RAWINPUT* rawinput, HWND hwnd) {
		// Handle keyboard input
		if(rawinput->header.dwType == RIM_TYPEKEYBOARD) {
			// If key down.
			if(rawinput->data.keyboard.Flags == RI_KEY_MAKE) {
				keycodes[rawinput->data.keyboard.VKey] = true;
			}
			// If key up.
			else if(rawinput->data.keyboard.Flags == RI_KEY_BREAK) {
				keycodes[rawinput->data.keyboard.VKey] = false;
			}
		}

		// Handle mouse input
		if(rawinput->header.dwType == RIM_TYPEMOUSE) {
			// Update mousebutton status
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)		mouseButtonStatus.left = true;
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)		mouseButtonStatus.right = true;
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)	mouseButtonStatus.middle = true;
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)			mouseButtonStatus.xbutton1 = true;
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)			mouseButtonStatus.xbutton2 = true;
			// Finish dragging if active on either left or right mousebutton
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
				mouseButtonStatus.left = false;
				if(leftMouseDragging) {
					leftMouseDragging = false;
					leftClickFrameDuration = 0;
				}
			}
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
				mouseButtonStatus.right = false;
				if(rightMouseDragging) {
					rightMouseDragging = false;
					rightClickFrameDuration = 0;
				}
			}
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)		mouseButtonStatus.middle = false;
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)			mouseButtonStatus.xbutton1 = false;
			if(rawinput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)			mouseButtonStatus.xbutton2 = false;

			if(mouseButtonStatus.left && !leftMouseDragging) leftClickFrameDuration++;
			if(mouseButtonStatus.right && !rightMouseDragging) rightClickFrameDuration++;

			// If any mousebutton has been held for 5 seconds, we're "dragging"
			// Once the drag starts we cache the current mouse position if the camera or 
			// anything else wants to use it later. 
			if(rightClickFrameDuration > 5 && !rightMouseDragging) {
				rightMouseDragging = true;
				rightDragCachedPos = clientMousePos;
				ClientToScreen(hwnd, &rightDragCachedPos);
			}
			if(leftClickFrameDuration > 5 && !leftMouseDragging) {
				leftMouseDragging = true;
				leftDragCachedPos = clientMousePos;
				ClientToScreen(hwnd, &leftDragCachedPos);
			}

			// Update deltamovement if we are dragging
			if(leftMouseDragging || rightMouseDragging) {
				if(deltaMouseMovement[0] == 0) deltaMouseMovement[0] = rawinput->data.mouse.lLastX;
				if(deltaMouseMovement[1] == 0) deltaMouseMovement[1] = rawinput->data.mouse.lLastY;
			}
		}
	}

	// Return the delta mouse movement, and reset for subsequent use
	inline std::array<int32_t, 2> ProcessDeltaMovement() {
		auto temp = deltaMouseMovement;
		deltaMouseMovement[0] = 0;
		deltaMouseMovement[1] = 0;
		return temp;
	}

	inline LRESULT WindowCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch(uMsg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_INPUT: {
			uint32_t inputSize = 0;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &inputSize, sizeof(RAWINPUTHEADER));
			static std::vector<std::byte> rawInput(inputSize);
			if(GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput[0], &inputSize, sizeof(RAWINPUTHEADER)) != inputSize) {
				return 1;
			}
			else {
				ProcessInput((RAWINPUT*)& rawInput[0], hwnd);
			}
			break;
		}
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			clientMousePos = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		}
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}