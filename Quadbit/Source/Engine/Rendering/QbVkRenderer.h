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

		template<typename T>
		RenderMeshComponent CreateMesh(std::vector<T> vertices, const std::vector<uint32_t>& indices, QbVkRenderMeshInstance* externalInstance) {
			return RenderMeshComponent{
				meshPipeline_->CreateVertexBuffer(vertices.data(), sizeof(T), static_cast<uint32_t>(vertices.size())),
				meshPipeline_->CreateIndexBuffer(indices),
				static_cast<uint32_t>(indices.size()),
				RenderMeshPushConstants{},
				externalInstance
			};
		}

		RenderTexturedObjectComponent CreateObject(const char* objPath, const char* texturePath) {
			return meshPipeline_->CreateObject(objPath, texturePath);
		}


		QbVkTexture LoadTexture(const char* imagePath, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, QbVkMemoryUsage memoryUsage, VkSamplerCreateInfo* samplerCreateInfo = nullptr,
			VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
		void CreateTexture(QbVkTexture& texture, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage, QbVkMemoryUsage memoryUsage, VkSamplerCreateInfo* samplerCreateInfo = nullptr,
			VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
		void CreateGPUBuffer(QbVkBuffer& buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage);
		void TransferDataToGPUBuffer(QbVkBuffer& buffer, VkDeviceSize size, const void* data);
		QbVkComputeInstance CreateComputeInstance(std::vector<QbComputeDescriptor>& descriptors, const char* shader, const char* shaderFunc,
			const VkSpecializationInfo* specInfo = nullptr, uint32_t pushConstantRangeSize = 0);
		QbVkRenderMeshInstance* CreateRenderMeshInstance(std::vector<QbRenderDescriptor> descriptors,
			std::vector<QbVkVertexInputAttribute> vertexAttribs, const char* vertexShader, const char* vertexEntry, const char* fragmentShader, const char* fragmentEntry);
		void ComputeDispatch(QbVkComputeInstance& instance);
		void DestroyComputeInstance(QbVkComputeInstance& instance);
		void DestroyMesh(const RenderMeshComponent& mesh);
		void DrawFrame();

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