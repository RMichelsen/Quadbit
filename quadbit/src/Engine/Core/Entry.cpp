#include "Entry.h"

#include "Engine/Application/Window.h"
#include "Engine/Core/Time.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Renderer.h"

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	// Make a window
	auto window = std::make_unique<Quadbit::Window>(hInstance, nCmdShow);
	// Init the renderer
	Quadbit::QbVkRenderer::Instance().Init(hInstance, window->hwnd_);

	auto* game = Quadbit::CreateGame();
	game->Init();
	while (window->ProcessMessages()) {
		// If the windows is minimized, skip rendering
		if (IsIconic(window->hwnd_)) continue;

		// Simulate and update the gamestate
		game->Simulate(Quadbit::Time::deltaTime);

		// Render the frame
		Quadbit::QbVkRenderer::Instance().DrawFrame();
	}

	delete game;
	Quadbit::QbVkRenderer::Instance().Shutdown();
	Quadbit::EntityManager::Instance().Shutdown();
	window.reset();
	
	return 0;
}