#pragma once
#include <functional>
#include <unordered_set>

#include "Engine/Rendering/QbVkShaderInstance.h"
#include "Engine/Core/QbRenderDefs.h"
#include "Engine/Core/QbVulkanDefs.h"

namespace Quadbit {
	// Forward declarations
	class MeshPipeline;
	class ImGuiPipeline;
	class ComputePipeline;
	class EntityManager;
	struct Entity;

	class QbVkRenderer {
	public:
		QbVkRenderer(HINSTANCE hInstance, HWND hwnd);
		~QbVkRenderer();

		// General
		void DrawFrame();
		float GetAspectRatio();
		QbVkBuffer CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage);
		void TransferDataToGPUBuffer(QbVkBuffer& buffer, VkDeviceSize size, const void* data);
		VkMemoryBarrier CreateMemoryBarrier(VkAccessFlags srcMask, VkAccessFlags dstMask);

		QbVkShaderInstance CreateShaderInstance();

		// Objects and Textures (Graphics)
		QbVkTexture LoadTexture(const char* imagePath, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, QbVkMemoryUsage memoryUsage, VkSamplerCreateInfo* samplerCreateInfo = nullptr,
			VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
		QbVkTexture CreateTexture(uint32_t width, uint32_t height, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage, QbVkMemoryUsage memoryUsage,
			VkSampler sampler = VK_NULL_HANDLE, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
		VkSampler CreateImageSampler(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
			float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod = 0.0f);

		// Compute Pipeline (Compute)
		QbVkComputeInstance* CreateComputeInstance(std::vector<QbVkComputeDescriptor>& descriptors, const char* shader, const char* shaderFunc,
			const VkSpecializationInfo* specInfo = nullptr, uint32_t pushConstantRangeSize = 0);
		void ComputeDispatch(QbVkComputeInstance* instance);
		void ComputeRecord(const QbVkComputeInstance* instance, std::function<void()> func);
		template<typename T>
		QbVkComputeDescriptor CreateComputeDescriptor(VkDescriptorType type, std::vector<T>& descriptors) {
			return { type, static_cast<uint32_t>(descriptors.size()), descriptors.data() };
		}
		QbVkComputeDescriptor CreateComputeDescriptor(VkDescriptorType type, void* descriptor);

		// Mesh Pipeline (Graphics)
		Entity GetActiveCamera();
		void RegisterCamera(Entity entity);
		const QbVkRenderMeshInstance* CreateRenderMeshInstance(std::vector<QbVkRenderDescriptor>& descriptors,
			std::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance,
			int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT);
		const QbVkRenderMeshInstance* CreateRenderMeshInstance(std::vector<QbVkVertexInputAttribute> vertexAttribs, QbVkShaderInstance& shaderInstance, 
			int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT);
		RenderTexturedObjectComponent CreateObject(const char* objPath, const char* texturePath, VkFormat textureFormat);
		template<typename T>
		RenderMeshComponent CreateMesh(std::vector<T> vertices, uint32_t vertexStride, const std::vector<uint32_t>& indices, const QbVkRenderMeshInstance* externalInstance,
			int pushConstantStride = -1) {

			return RenderMeshComponent{
				CreateVertexBuffer(vertices.data(), vertexStride, static_cast<uint32_t>(vertices.size())),
				CreateIndexBuffer(indices),
				static_cast<uint32_t>(indices.size()),
				std::array<float, 32>(),
				pushConstantStride,
				externalInstance
			};
		}
		RenderMeshComponent CreateMesh(const char* objPath, std::vector<QbVkVertexInputAttribute> vertexModel, const QbVkRenderMeshInstance* externalInstance,
			int pushConstantStride = -1);
		void DestroyMesh(RenderMeshComponent& renderMeshComponent);
		VertexBufHandle CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount);
		IndexBufHandle CreateIndexBuffer(const std::vector<uint32_t>& indices);
		template<typename T>
		QbVkRenderDescriptor CreateRenderDescriptor(VkDescriptorType type, std::vector<T>& data, VkShaderStageFlagBits shaderStage) {
			return { type, static_cast<uint32_t>(data.size()), data.data(), shaderStage };
		}
		QbVkRenderDescriptor CreateRenderDescriptor(VkDescriptorType type, void* descriptor, VkShaderStageFlagBits shaderStage);

		// Sky gradient setup
		void LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour);

	private:
		bool canRender_ = false;

		HINSTANCE localHandle_ = NULL;
		HWND windowHandle_ = NULL;

		EntityManager& entityManager_;

		VkInstance instance_ = VK_NULL_HANDLE;
		std::unique_ptr<QbVkContext> context_;
		std::unique_ptr<MeshPipeline> meshPipeline_;
		std::unique_ptr<ImGuiPipeline> imGuiPipeline_;
		std::unique_ptr<ComputePipeline> computePipeline_;

		UserAllocations userAllocations_{};

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