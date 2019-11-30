#pragma once
#include <EASTL/unique_ptr.h>
#include <Windows.h>

#include "Game.h"

namespace Quadbit {
	extern eastl::unique_ptr<Game> CreateGame();
}

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow);
