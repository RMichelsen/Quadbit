#include "Window.h"

#include <windowsx.h>
#include <imgui/imgui.h>

#include "Engine/Application/InputHandler.h"
#include "Engine/Core/Time.h"
#include "Engine/Core/Logging.h"

namespace Quadbit {
	Window::Window(HINSTANCE hInstance, int nCmdShow) {
		this->instance_ = hInstance;

		// No Windows bitmap-stretching
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

		if (!SetupWindow(nCmdShow, WndProc)) {
			QB_LOG_ERROR("Failed to setup win32 Window\n");
			return;
		}
		if (!RegisterRawInputDevices(&RAW_INPUT_DEVICES[0], 2, sizeof(RAWINPUTDEVICE))) {
			QB_LOG_ERROR("Failed to register input devices\n");
			return;
		}
		if (!SetupConsole()) {
			QB_LOG_ERROR("Failed to setup the console\n");
			return;
		}
	}

	Window::~Window() {
		DestroyWindow(hwnd_);
		UnregisterClass("Quadbit_Class", instance_);
	}

	bool Window::ProcessMessages() {
		// Start a new ImGui frame
		ImGui::NewFrame();
		Time::UpdateTimer();
		InputHandler::Instance().NewFrame();
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT) {
				return false;
			}
		}
		return true;
	}

	bool Window::SetupWindow(int nCmdShow, WNDPROC windowProc) {
		// Could make these parameters but fine as const for now
		const char* windowClassName = "Quadbit_Class";
		const char* windowTitle = "Quadbit Engine";

		WNDCLASSEX windowClass = {};
		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = windowProc;
		windowClass.hInstance = instance_;
		windowClass.lpszClassName = windowClassName;
		windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);

		// Register window class
		if (!RegisterClassEx(&windowClass)) {
			return false;
		}

		// Create window
		hwnd_ = CreateWindowEx(
			0,
			windowClassName,
			windowTitle,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			2560, // Resolution width
			1440, // Resolution height
			NULL,
			NULL,
			instance_,
			this
		);
		if (hwnd_ == NULL) return false;
		ShowWindow(hwnd_, nCmdShow);

		return true;
	}

	bool Window::SetupConsole() {
		if (!AllocConsole()) return false;
		if (!freopen("CONIN$", "r", stdin)) return false;
		if (!freopen("CONOUT$", "w", stdout)) return false;
		if (!freopen("CONOUT$", "w", stderr)) return false;
		return true;
	}

	LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		Window* window;
		if (uMsg == WM_NCCREATE) {
			window = static_cast<Window*>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
			SetLastError(0);
			if (!SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window))) {
				if (GetLastError() != 0) return FALSE;
			}
		}
		else
			window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		switch (uMsg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_INPUT: {
			uint32_t inputSize = 0;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &inputSize, sizeof(RAWINPUTHEADER));
			LPBYTE rawInput = new BYTE[inputSize];
			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, rawInput, &inputSize, sizeof(RAWINPUTHEADER)) != inputSize) {
				QB_LOG_ERROR("GetRawInputData didn't return correct size!\n");
			}
			else {
				InputHandler::Instance().ProcessRawInput((RAWINPUT*)rawInput, window->hwnd_);
				delete[] rawInput;
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
			InputHandler::Instance().mousePos_ = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		}
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}