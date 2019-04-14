#include <PCH.h>
#include <ImGui/imgui.h>

#include "Engine/Global/InputHandler.h"
#include "Engine/Global/ImGuiState.h"
#include "Engine/Global/Time.h"
#include "Engine/Application/Window.h"
#include "Engine/Rendering/Camera.h"
#include "Engine/Rendering/QbVkRenderer.h"

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	auto window = std::make_unique<Window>(hInstance, nCmdShow, InputHandler::WindowCallback);
	auto renderer = std::make_unique<QbVkRenderer>(hInstance, window->hwnd_);

	while(window->ProcessMessages()) {
		// If the windows is minimized, skip rendering
		if(IsIconic(window->hwnd_)) continue;
		Camera::UpdateCamera();
		if(renderer->canRender_) {
			renderer->DrawFrame();
		}
	}

	return 0;
}