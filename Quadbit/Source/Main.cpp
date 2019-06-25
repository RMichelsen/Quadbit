#include <PCH.h>

#include "Engine/Core/InputHandler.h"
#include "Engine/Core/Time.h"
#include "Engine/Application/Window.h"
#include "Game/Water.h"

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	auto window = std::make_unique<Quadbit::Window>(hInstance, nCmdShow, Quadbit::InputHandler::WindowCallback);
	auto game = std::make_unique<Water>(hInstance, window->hwnd_);

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