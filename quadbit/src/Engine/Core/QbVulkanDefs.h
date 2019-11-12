#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

namespace Quadbit {
	// VULKAN DEFINITIONS
	using VertexBufHandle = uint16_t;
	using IndexBufHandle = uint16_t;

	enum class QbVkMemoryUsage {
		QBVK_MEMORY_USAGE_UNKNOWN,
		QBVK_MEMORY_USAGE_CPU_ONLY,
		QBVK_MEMORY_USAGE_GPU_ONLY,
		QBVK_MEMORY_USAGE_CPU_TO_GPU,
		QBVK_MEMORY_USAGE_GPU_TO_CPU
	};

	enum class QbVkVertexInputAttribute {
		QBVK_VERTEX_ATTRIBUTE_POSITION,
		QBVK_VERTEX_ATTRIBUTE_NORMAL,
		QBVK_VERTEX_ATTRIBUTE_UV,
		QBVK_VERTEX_ATTRIBUTE_COLOUR,
		QBVK_VERTEX_ATTRIBUTE_FLOAT,
		QBVK_VERTEX_ATTRIBUTE_FLOAT4
	};

	struct QbVkPool;
	struct QbVkAllocation {
		QbVkPool* pool = nullptr;
		uint32_t id = 0;
		VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
		VkDeviceSize offset = 0;
		VkDeviceSize size = 0;
		unsigned char* data = nullptr;
	};

	struct QbVkBuffer {
		VkBuffer buf = VK_NULL_HANDLE;
		QbVkAllocation alloc{};
		VkDescriptorBufferInfo descriptor{};
	};

	struct QbVkAsyncBuffer {
		VkBuffer buf = VK_NULL_HANDLE;
		QbVkAllocation alloc{};
		VkDescriptorBufferInfo descriptor{};
		VkCommandBuffer copyCommandBuffer = VK_NULL_HANDLE;
		QbVkBuffer stagingBuffer{};
		VkFence fence = VK_NULL_HANDLE;
		bool ready;
	};

	struct QbVkImage {
		VkImage imgHandle = VK_NULL_HANDLE;
		QbVkAllocation alloc{};
	};

	struct QbVkTexture {
		QbVkImage image{};
		VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageView imageView = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		VkDescriptorImageInfo descriptor;
		VkFormat format = VK_FORMAT_UNDEFINED;
	};

	struct QbVkComputeDescriptor {
		VkDescriptorType type;
		uint32_t count;
		void* data;
	};

	struct QbVkRenderDescriptor {
		VkDescriptorType type;
		uint32_t count;
		void* data;
		VkShaderStageFlagBits shaderStage;
	};

	struct QbVkModel {
		std::vector<float> vertices;
		uint32_t vertexStride;
		std::vector<uint32_t> indices;
	};

	struct QbVkComputeInstance {
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkQueryPool queryPool = VK_NULL_HANDLE;

		double msAvgTime = 0.0;
	};

	enum class QbVkAllocationType {
		QBVK_ALLOCATION_TYPE_UNKNOWN,
		QBVK_ALLOCATION_TYPE_FREE,
		QBVK_ALLOCATION_TYPE_BUFFER,
		QBVK_ALLOCATION_TYPE_IMAGE_UNKNOWN,
		QBVK_ALLOCATION_TYPE_IMAGE_LINEAR,
		QBVK_ALLOCATION_TYPE_IMAGE_OPTIMAL,
	};

	struct GPU {
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

		VkSurfaceCapabilitiesKHR surfaceCapabilities{};
		VkPhysicalDeviceProperties deviceProps{};
		VkPhysicalDeviceMemoryProperties memoryProps{};
		VkPhysicalDeviceFeatures features{};

		std::vector<VkSurfaceFormatKHR> surfaceFormats;
		std::vector<VkPresentModeKHR> presentModes;

		std::vector<VkQueueFamilyProperties> queueProps;
		std::vector<VkExtensionProperties> extensionProps;

		int graphicsFamilyIdx = -1;
		int presentFamilyIdx = -1;
		int computeFamilyIdx = -1;
	};

	struct Swapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkFormat imageFormat{};
		VkPresentModeKHR presentMode{};
		VkExtent2D extent{};
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
	};

	struct MSAAResources {
		VkSampleCountFlagBits msaaSamples;
		QbVkImage msaaImage;
		VkImageView msaaImageView;
	};

	struct DepthResources {
		QbVkImage depthImage{};
		VkImageView imageView = VK_NULL_HANDLE;
	};

	struct RenderingResources {
		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		VkCommandBuffer commandbuffer = VK_NULL_HANDLE;
		VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
		VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
	};

	class QbVkAllocator;
	struct QbVkContext {
		std::unique_ptr<GPU> gpu;
		std::unique_ptr<QbVkAllocator> allocator;
		VkDevice device = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		VkQueue presentQueue = VK_NULL_HANDLE;
		VkQueue computeQueue = VK_NULL_HANDLE;

		MSAAResources multisamplingResources;
		DepthResources depthResources{};
		Swapchain swapchain{};

		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkRenderPass mainRenderPass = VK_NULL_HANDLE;

		std::array<RenderingResources, MAX_FRAMES_IN_FLIGHT> renderingResources;
	};

	struct QbVkRenderMeshInstance {
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;

		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};
	};
}