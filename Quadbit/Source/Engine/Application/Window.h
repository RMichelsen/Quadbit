#pragma once

namespace Quadbit {
	class Window {
	public:
		HWND hwnd_ = NULL;
		HINSTANCE instance_ = NULL;

		Window(HINSTANCE hInstance, int nCmdShow, WNDPROC windowProc);
		~Window();
		bool ProcessMessages();

	private:
		bool SetupWindow(int nCmdShow, WNDPROC windowProc);
		bool SetupConsole();
	};
}