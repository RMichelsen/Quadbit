#pragma once
#include <Windows.h>
#include <memory>

#include "Game.h"

namespace Quadbit {
	extern Game* CreateGame();
}

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow);
