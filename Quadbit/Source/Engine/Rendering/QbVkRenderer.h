#pragma once
#include <functional>
#include <variant>

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
		void CreateGPUBuffer(QbVkBuffer& buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage);
		void TransferDataToGPUBuffer(QbVkBuffer& buffer, VkDeviceSize size, const void* data);
		VkMemoryBarrier CreateMemoryBarrier(VkAccessFlags srcMask, VkAccessFlags dstMask);

		// Objects and Textures
		void LoadEnvironmentMap(const char* environmentTexture, VkFormat textureFormat);
		VkDescriptorImageInfo GetEnvironmentMapDescriptor();
		QbVkTexture LoadCubemap(const char* imagePath, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, QbVkMemoryUsage memoryUsage);
		QbVkTexture LoadTexture(const char* imagePath, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, QbVkMemoryUsage memoryUsage, VkSamplerCreateInfo* samplerCreateInfo = nullptr,
			VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
		void CreateTexture(QbVkTexture& texture, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageLayout imageLayout,
			VkImageAspectFlags imageAspectFlags, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage, QbVkMemoryUsage memoryUsage, 
			VkSampler sampler = VK_NULL_HANDLE, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
		VkSampler CreateImageSampler(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
			float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod = 0.0f);


		// Compute Pipeline
		std::shared_ptr<QbVkComputeInstance> CreateComputeInstance(std::vector<QbVkComputeDescriptor>& descriptors, const char* shader, const char* shaderFunc,
			const VkSpecializationInfo* specInfo = nullptr, uint32_t pushConstantRangeSize = 0);
		void ComputeDispatch(std::shared_ptr<QbVkComputeInstance> instance);
		void ComputeRecord(std::shared_ptr<QbVkComputeInstance> instance, std::function<void()> func);
		template<typename T>
		QbVkComputeDescriptor CreateComputeDescriptor(VkDescriptorType type, std::vector<T>& data) {
			return { type, static_cast<uint32_t>(data.size()), data.data() };
		}
		QbVkComputeDescriptor CreateComputeDescriptor(VkDescriptorType type, std::variant<VkDescriptorImageInfo, VkDescriptorBufferInfo> data);


		// Mesh Pipeline
		Entity GetActiveCamera();
		void RegisterCamera(Entity entity);
		std::shared_ptr<QbVkRenderMeshInstance> CreateRenderMeshInstance(std::vector<QbVkRenderDescriptor>& descriptors,
			std::vector<QbVkVertexInputAttribute> vertexAttribs, const char* vertexShader, const char* vertexEntry, const char* fragmentShader, const char* fragmentEntry,
			int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT);
		std::shared_ptr<QbVkRenderMeshInstance> CreateRenderMeshInstance(std::vector<QbVkVertexInputAttribute> vertexAttribs, const char* vertexShader, const char* vertexEntry,
			const char* fragmentShader, const char* fragmentEntry, int pushConstantStride = -1, VkShaderStageFlags pushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT);
		RenderTexturedObjectComponent CreateObject(const char* objPath, const char* texturePath, VkFormat textureFormat);
		template<typename T>
		RenderMeshComponent CreateMesh(std::vector<T> vertices, uint32_t vertexStride, const std::vector<uint32_t>& indices, std::shared_ptr<QbVkRenderMeshInstance> externalInstance,
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
		RenderMeshComponent CreateMesh(const char* objPath, std::vector<QbVkVertexInputAttribute> vertexModel, std::shared_ptr<QbVkRenderMeshInstance> externalInstance,
			int pushConstantStride = -1);
		void DestroyMesh(RenderMeshComponent& renderMeshComponent);
		VertexBufHandle CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount);
		IndexBufHandle CreateIndexBuffer(const std::vector<uint32_t>& indices);
		template<typename T>
		QbVkRenderDescriptor CreateRenderDescriptor(VkDescriptorType type, std::vector<T>& data, VkShaderStageFlagBits shaderStage) {
			return { type, static_cast<uint32_t>(data.size()), data.data(), shaderStage };
		}
		QbVkRenderDescriptor CreateRenderDescriptor(VkDescriptorType type, std::variant<VkDescriptorImageInfo, VkDescriptorBufferInfo> data, VkShaderStageFlagBits shaderStage);
		

	private:
		bool canRender_ = false;

		HINSTANCE localHandle_ = NULL;
		HWND windowHandle_ = NULL;

		EntityManager& entityManager_;

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

		void ImGuiUpdateContent();
		void PrepareFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer, VkFramebuffer& framebuffer, VkImageView imageView);
	};
}