#pragma once
#include <Windows.h>
#include <EASTL/unique_ptr.h>

#include "Engine/Application/InputHandler.h"

namespace Quadbit {
	class Window {
	public:
		HWND hwnd_ = NULL;
		HINSTANCE instance_ = NULL;

		eastl::unique_ptr<InputHandler> inputHandler_;

		Window(HINSTANCE hInstance, int nCmdShow);
		~Window();

		bool ProcessMessages();

	private:
		bool SetupWindow(int nCmdShow, WNDPROC windowProc);
		bool SetupConsole();
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	};
}