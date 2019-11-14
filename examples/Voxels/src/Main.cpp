#include <Windows.h>
#include <memory>

#include "Engine/Application/Window.h"
#include "Engine/Core/Time.h"

#include "Infinitum.h"
#include "Utils/MagicaVoxImporter.h"

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	auto window = std::make_unique<Quadbit::Window>(hInstance, nCmdShow);
	auto game = std::make_unique<Infinitum>(hInstance, window->hwnd_);

	game->Init();

	//game->Test();

	while(window->ProcessMessages()) {
		// If the windows is minimized, skip rendering
		if(IsIconic(window->hwnd_)) continue;

		// Simulate and update the gamestate
		game->Simulate(Quadbit::Time::deltaTime);

		// Render the frame
		game->DrawFrame();
	}

	game.reset();
	window.reset();

	return 0;
}