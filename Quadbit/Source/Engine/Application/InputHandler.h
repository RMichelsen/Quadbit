#pragma once

#include <EASTL/array.h>

#include "Engine/Application/InputTypes.h"

namespace Quadbit {
	class InputHandler {
	public:
		POINT mousePos_{};
		MouseDelta mouseDelta_{};
		MouseButtonStatus mouseButtonActive_{};
		MouseButtonStatus mouseButtonPressed_{};
		MouseDragStatus mouseDrag_{};

		eastl::array<unsigned char, 0xFF> keyState_;
		eastl::array<unsigned char, 0xFF> keyPressed_;
		KeyboardControlKeys controlKeysPressed_{};

		void NewFrame();
		void ProcessRawInput(RAWINPUT* rawInput, HWND hwnd);
	};
}