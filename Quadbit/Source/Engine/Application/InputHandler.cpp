#include <Windows.h>

#include "InputHandler.h"

namespace Quadbit {
	void InputHandler::NewFrame() {
		// Reset per-frame key/button press
		memset(&mouseDelta_, 0, sizeof(MouseDelta));
		memset(&mouseButtonPressed_, 0, sizeof(MouseButtonStatus));
		memset(&controlKeysPressed_, 0, sizeof(KeyboardControlKeys));
		keyPressed_.fill(0);
	}

	void InputHandler::ProcessRawInput(RAWINPUT* rawInput, HWND hwnd) {
		// Handle keyboard input
		if (rawInput->header.dwType == RIM_TYPEKEYBOARD) {
			const auto virtual_key = rawInput->data.keyboard.VKey;

			// If key down.
			if (rawInput->data.keyboard.Message == WM_KEYDOWN ||
				rawInput->data.keyboard.Message == WM_SYSKEYDOWN) {
				keyState_[rawInput->data.keyboard.VKey] = 0x80;
				keyPressed_[rawInput->data.keyboard.VKey] = true;

				if (virtual_key == VK_DELETE) controlKeysPressed_.del = true;
				if (virtual_key == VK_BACK) controlKeysPressed_.backspace = true;
				if (virtual_key == VK_RETURN) controlKeysPressed_.enter = true;
				if (virtual_key == VK_TAB) controlKeysPressed_.tab = true;
				if (virtual_key == VK_LEFT) controlKeysPressed_.left = true;
				if (virtual_key == VK_RIGHT) controlKeysPressed_.right = true;
				if (virtual_key == VK_UP) controlKeysPressed_.up = true;
				if (virtual_key == VK_DOWN) controlKeysPressed_.down = true;
			}
			// If key up.
			else if (rawInput->data.keyboard.Message == WM_KEYUP ||
				rawInput->data.keyboard.Message == WM_SYSKEYUP) {
				keyState_[virtual_key] = 0;
			}
		}

		// Handle mouse input
		if (rawInput->header.dwType == RIM_TYPEMOUSE) {
			// Update mousebutton status
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
				mouseButtonActive_.left = true;
				mouseButtonPressed_.left = true;
			}
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
				mouseButtonActive_.right = true;
				mouseButtonPressed_.right = true;
			}
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
				mouseButtonActive_.middle = true;
				mouseButtonPressed_.middle = true;
			}
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) {
				mouseButtonActive_.xbutton1 = true;
				mouseButtonPressed_.xbutton1 = true;
			}
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) {
				mouseButtonActive_.xbutton2 = true;
				mouseButtonPressed_.xbutton2 = true;
			}
			// Finish dragging if active on either left or right mousebutton
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
				mouseButtonActive_.left = false;
				if (mouseDrag_.left.active) {
					mouseDrag_.left.active = false;
					mouseDrag_.left.duration = 0;
				}
			}
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
				mouseButtonActive_.right = false;
				if (mouseDrag_.right.active) {
					mouseDrag_.right.active = false;
					mouseDrag_.right.duration = 0;
				}
			}
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)		mouseButtonActive_.middle = false;
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)			mouseButtonActive_.xbutton1 = false;
			if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)			mouseButtonActive_.xbutton2 = false;

			if (mouseButtonActive_.left && !mouseDrag_.left.active) mouseDrag_.left.duration++;
			if (mouseButtonActive_.right && !mouseDrag_.right.active) mouseDrag_.right.duration++;

			// If any mousebutton has been held for 5 frames, we're "dragging"
			// Once the drag starts we cache the current mouse position if the camera or 
			// anything else wants to use it later. 
			if (mouseDrag_.right.duration > 5 && !mouseDrag_.right.active) {
				mouseDrag_.right.active = true;
				mouseDrag_.right.cachedPos = mousePos_;
				ClientToScreen(hwnd, &mouseDrag_.right.cachedPos);
			}
			if (mouseDrag_.left.duration > 5 && !mouseDrag_.left.active) {
				mouseDrag_.left.active = true;
				mouseDrag_.left.cachedPos = mousePos_;
				ClientToScreen(hwnd, &mouseDrag_.left.cachedPos);
			}

			// Update deltamovement if we are dragging
			if (mouseDrag_.left.active || mouseDrag_.right.active) {
				mouseDelta_.x = static_cast<float>(rawInput->data.mouse.lLastX);
				mouseDelta_.y = static_cast<float>(rawInput->data.mouse.lLastY);
			}
		}
	}

}
