#pragma once

#include <Windows.h>

namespace Quadbit {
	class Window {
	public:
		HWND hwnd_ = NULL;
		HINSTANCE instance_ = NULL;

		Window(HINSTANCE hInstance, int nCmdShow);
		~Window();

		bool ProcessMessages();

	private:
		bool SetupWindow(int nCmdShow, WNDPROC windowProc);
		bool SetupConsole();
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	};
}