#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <variant>
#include <functional>
#include <unordered_set>
#include <type_traits>
#include <deque>
#include <cassert>

inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

namespace Quadbit {
	template<typename T>
	struct QbVkResourceHandle {
		uint32_t index : 16;
		uint32_t version : 16;
	};

	template<typename T, size_t Size>
	struct QbVkResource {
		uint32_t resourceIndex = 0;
		std::array<T, Size> elements{};
		std::array<uint16_t, Size> versions{};
		std::deque<uint16_t> freeList;

		template<typename U>
		T& operator[](QbVkResourceHandle<U> handle) {
			static_assert(std::is_same<T, U>::value, "Resource has a different type than the resource handle passed!");
			assert(handle.index < Size && "Resource array out of bounds!");
			assert(handle.version == versions[handle.index] && "Handle passed does not match internal handle!");
			return elements[handle.index];
		}

		template<typename U>
		void DestroyResource(QbVkResourceHandle<U> handle) {
			static_assert(std::is_same<T, U>::value, "Resource has a different type than the resource handle passed!");
			assert(handle.index < Size && "Resource array out of bounds!");
			assert(handle.version == versions[handle.index] && "Handle passed does not match internal handle!");
			versions[handle.index]++;
			freeList.push_back(handle.index);
		}

		QbVkResourceHandle<T> GetHandle(uint16_t index) {
			return QbVkResourceHandle<T> { index, versions[index] };
		}

		QbVkResourceHandle<T> GetNextHandle() {
			uint16_t next = resourceIndex;
			if (freeList.empty()) {
				resourceIndex++;
			}
			else {
				next = freeList.front();
				freeList.pop_front();
			}
			assert(next < (Size - 1) && "Resource array full, too many elements!");
			return QbVkResourceHandle<T> {next, versions[next]};
		}
	};

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

	class QbVkRenderer;
	struct QbVkBuffer {
		VkBuffer buf = VK_NULL_HANDLE;
		QbVkAllocation alloc{};
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
		VkFramebuffer frameBuffer = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
		VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
		VkSemaphore transferSemaphore = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
	};

	class QbVkAllocator;
	class QbVkResourceManager;
	class EntityManager;
	class InputHandler;
	struct QbVkContext {
		InputHandler* inputHandler;
		EntityManager* entityManager;

		std::unique_ptr<GPU> gpu;
		std::unique_ptr<QbVkAllocator> allocator;
		std::unique_ptr<QbVkResourceManager> resourceManager;
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

	using QbVkTextureHandle = QbVkResourceHandle<QbVkTexture>;
	using QbVkBufferHandle = QbVkResourceHandle<QbVkBuffer>;

	struct QbVkTransfer {
		VkDeviceSize size;
		QbVkBufferHandle destinationBuffer;
	};

	template<typename T>
	struct QbMappedGPUBuffer {
		QbVkBufferHandle handle;
		T* data;
		VkDescriptorBufferInfo descriptor{};

		T* operator->() {
			return data;
		}
	};

	struct QbGPUBuffer {
		QbVkBufferHandle handle;
		VkDescriptorBufferInfo descriptor{};
	};

	struct QbTexture {
		QbVkTextureHandle handle;
		VkDescriptorImageInfo descriptor{};
	};
}