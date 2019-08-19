#include <PCH.h>
#include "Window.h"

#include "Engine/Core/Time.h"
#include "Engine/Core/InputHandler.h"

namespace Quadbit {
	Window::Window(HINSTANCE hInstance, int nCmdShow, WNDPROC windowProc) {
		this->instance_ = hInstance;

		// No Windows bitmap-stretching
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

		if(!SetupWindow(nCmdShow, windowProc)) {
			QB_LOG_ERROR("Failed to setup win32 Window\n");
			return;
		}
		if(!InputHandler::Initialize()) {
			QB_LOG_ERROR("Failed to setup the input handler\n");
			return;
		}
		if(!SetupConsole()) {
			QB_LOG_ERROR("Failed to setup the console\n");
			return;
		}
	}

	Window::~Window() {
		DestroyWindow(hwnd_);
		UnregisterClass("Quadbit_Class", instance_);
	}

	bool Window::ProcessMessages() {
		Time::UpdateTimer();
		InputHandler::Update();
		MSG msg;
		while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if(msg.message == WM_QUIT) {
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
		if(!RegisterClassEx(&windowClass)) {
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
			NULL
		);
		if(hwnd_ == NULL) return false;
		ShowWindow(hwnd_, nCmdShow);

		return true;
	}

	bool Window::SetupConsole() {
		if(!AllocConsole()) return false;
		if(!freopen("CONIN$", "r", stdin)) return false;
		if(!freopen("CONOUT$", "w", stdout)) return false;
		if(!freopen("CONOUT$", "w", stderr)) return false;
		return true;
	}
}