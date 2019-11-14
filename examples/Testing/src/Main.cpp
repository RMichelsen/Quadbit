#include <Windows.h>
#include <memory>

#include "Engine/Core/Time.h"
#include "Engine/Application/Window.h"
#include "Game.h"

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	auto window = std::make_unique<Quadbit::Window>(hInstance, nCmdShow);
	auto game = std::make_unique<Game>(hInstance, window->hwnd_);

	game->Init();
	
	while(window->ProcessMessages()) {
		// If the windows is minimized, skip rendering
		if(IsIconic(window->hwnd_)) continue;

		// Simulate and update the gamestate
		game->Simulate(Quadbit::Time::deltaTime);

		// Render the frame
		game->DrawFrame();
	}

	return 0;
}