#pragma once

#include "Common/QbVkCommon.h"
#include "Pipelines/MeshPipeline.h"
#include "Pipelines/ImGuiPipeline.h"
#include "Memory/QbVkAllocator.h"
#include "../Entities/EntityManager.h"

namespace Quadbit {
	class QbVkRenderer {
	public:
		bool canRender_ = false;

		QbVkRenderer(HINSTANCE hInstance, HWND hwnd, std::shared_ptr<Quadbit::EntityManager> registry);
		~QbVkRenderer();

		void DrawFrame();

	private:
		HINSTANCE localHandle_ = NULL;
		HWND windowHandle_ = NULL;

		std::shared_ptr<Quadbit::EntityManager> entityManager_;

		VkInstance instance_ = VK_NULL_HANDLE;
		std::shared_ptr<QbVkContext> context_ = std::make_shared<QbVkContext>();
		std::unique_ptr<MeshPipeline> meshPipeline_ = nullptr;
		std::unique_ptr<ImGuiPipeline> imGuiPipeline_ = nullptr;

		// DEBUG BUILD ONLY
#ifdef QBDEBUG
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

		void CreateDepthResources();
		void CreateSwapChain();
		void RecreateSwapchain();
		void CreateMainRenderPass();

		void PrepareFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer, VkFramebuffer& framebuffer, VkImageView imageView);
	};
}