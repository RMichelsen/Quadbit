#pragma once

#include "Engine/Application/QbInputDefs.h"

namespace Quadbit {
	class InputHandler {
	public:
		POINT mousePos_{};
		MouseDelta mouseDelta_{};
		MouseButtonStatus mouseButtonActive_{};
		MouseButtonStatus mouseButtonPressed_{};
		MouseDragStatus mouseDrag_{};

		unsigned char keyState_[0xFF];

		KeyboardControlKeys controlKeysPressed_{};
		unsigned char keyPressed_[0xFF];

		static InputHandler& Instance();
		void NewFrame();
		void ProcessRawInput(RAWINPUT* rawInput, HWND hwnd);
	};
}