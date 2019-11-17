#pragma once
#include <functional>
#include <unordered_set>

#include "Engine/Rendering/ShaderInstance.h"
#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	// Forward declarations
	class MeshPipeline;
	class ImGuiPipeline;
	class ComputePipeline;
	class EntityManager;
	struct Entity;

	class QbVkRenderer {
	public:
		QbVkRenderer(HINSTANCE hInstance, HWND hwnd, InputHandler* const inputHandler, EntityManager* const entityManager);
		~QbVkRenderer();

		void DrawFrame();
		void DestroyResource(QbVkBuffer buffer);
		void DestroyResource(QbVkTexture texture);

	private:
		// API Classes can access internals to provide functionality to the user
		friend class Graphics;
		friend class Compute;

		bool canRender_ = false;

		HINSTANCE localHandle_ = NULL;
		HWND windowHandle_ = NULL;

		VkInstance instance_ = VK_NULL_HANDLE;
		std::unique_ptr<QbVkContext> context_;
		std::unique_ptr<MeshPipeline> meshPipeline_;
		std::unique_ptr<ImGuiPipeline> imGuiPipeline_;
		std::unique_ptr<ComputePipeline> computePipeline_;

		//QbVkUserAllocations userAllocations_{};

		// DEBUG BUILD ONLY
#ifndef NDEBUG
		VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
		void CreateDebugMessenger();
#endif

		void CreateInstance();
		void CreateSurface();
		void CreatePhysicalDevice();
		void CreateLogicalDeviceAndQueues();

		void CreateCommandPool();
		void CreateSyncObjects();
		void AllocateCommandBuffers();

		void CreateMultisamplingResources();
		void CreateDepthResources();
		void CreateSwapChain();
		void RecreateSwapchain();
		void CreateMainRenderPass();

		void ImGuiUpdateContent();
		void PrepareFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer, VkFramebuffer& framebuffer, VkImageView imageView);
	};
}