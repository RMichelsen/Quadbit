#include "Entry.h"

#include "Engine/Application/Window.h"
#include "Engine/Core/Time.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Renderer.h"
#include "Engine/API/Compute.h"

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	auto window = std::make_unique<Quadbit::Window>(hInstance, nCmdShow);
	auto entityManager = std::make_unique<Quadbit::EntityManager>();
	auto renderer = std::make_unique<Quadbit::QbVkRenderer>(hInstance, window->hwnd_, window->inputHandler_.get(), entityManager.get());

	auto* game = Quadbit::CreateGame();
	game->input_ = window->inputHandler_.get();
	game->entityManager_ = entityManager.get();
	game->compute_ = new Quadbit::Compute(renderer.get());
	game->graphics_ = new Quadbit::Graphics(renderer.get());

	game->Init();
	while (window->ProcessMessages()) {
		// If the windows is minimized, skip rendering
		if (IsIconic(window->hwnd_)) continue;

		// Simulate and update the gamestate
		game->Simulate(Quadbit::Time::deltaTime);

		// Render the frame
		renderer->DrawFrame();
	}

	delete game;

	renderer.reset();
	entityManager.reset();
	window.reset();
	
	return 0;
}