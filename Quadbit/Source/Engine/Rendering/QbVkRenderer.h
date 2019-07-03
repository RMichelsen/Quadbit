#pragma once

#include "Common/QbVkDefines.h"
#include "Pipelines/MeshPipeline.h"
#include "Pipelines/ImGuiPipeline.h"
#include "Pipelines/ComputePipeline.h"
#include "Memory/QbVkAllocator.h"
#include "../Entities/EntityManager.h"

namespace Quadbit {
	class QbVkRenderer {
	public:
		QbVkRenderer(HINSTANCE hInstance, HWND hwnd);
		~QbVkRenderer();

		float GetAspectRatio();
		void RegisterCamera(Entity entity);
		RenderMeshComponent CreateMesh(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices, QbVkRenderMeshInstance* externalInstance = nullptr);
		QbVkComputeInstance CreateComputeInstance(std::vector<std::tuple<VkDescriptorType, void*>> descriptors, const char* shader, const char* shaderFunc, const VkSpecializationInfo* specInfo = nullptr);
		void UpdateComputeDescriptors(std::vector<std::tuple<VkDescriptorType, void*>> descriptors, QbVkComputeInstance& instance);
		QbVkRenderMeshInstance* CreateRenderMeshInstance(std::vector<std::tuple<VkDescriptorType, void*, VkShaderStageFlagBits>> descriptors,
			const char* vertexShader, const char* vertexEntry, const char* fragmentShader, const char* fragmentEntry);
		void ComputeDispatch(QbVkComputeInstance& instance);
		void DestroyComputeInstance(QbVkComputeInstance& instance);
		void DestroyMesh(const RenderMeshComponent& mesh);
		void DrawFrame();

		// TODO: Questionable engine design, cleanup?
		std::shared_ptr<QbVkContext> RequestRenderContext();
		VkInstance GetInstance();

	private:
		bool canRender_ = false;

		HINSTANCE localHandle_ = NULL;
		HWND windowHandle_ = NULL;

		EntityManager* entityManager_;

		VkInstance instance_ = VK_NULL_HANDLE;
		std::shared_ptr<QbVkContext> context_ = std::make_shared<QbVkContext>();
		std::unique_ptr<MeshPipeline> meshPipeline_ = nullptr;
		std::unique_ptr<ImGuiPipeline> imGuiPipeline_ = nullptr;
		std::unique_ptr<ComputePipeline> computePipeline_ = nullptr;

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

		void CreateMultisamplingResources();
		void CreateDepthResources();
		void CreateSwapChain();
		void RecreateSwapchain();
		void CreateMainRenderPass();

		void PrepareFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer, VkFramebuffer& framebuffer, VkImageView imageView);
	};
}