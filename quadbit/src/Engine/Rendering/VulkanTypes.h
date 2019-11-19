#pragma once

#include <cassert>

#include <EASTL/array.h>
#include <EASTL/deque.h>
#include <EASTL/type_traits.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>


inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

namespace Quadbit {
	// At the moment we only support 65535 deletions of the same resource slot
	// And 65535 unique resources of the same type

	template<typename T>
	struct QbVkResourceHandle {
		uint32_t index : 16;
		uint32_t version : 16;
	};

	template<typename T, size_t Size>
	struct QbVkResource {
		uint16_t resourceIndex = 0;
		eastl::array<T, Size> elements{};
		eastl::array<uint16_t, Size> versions{};
		eastl::deque<uint16_t> freeList;

		template<typename U>
		T& operator[](QbVkResourceHandle<U> handle) {
			static_assert(eastl::is_same<T, U>::value, "Resource has a different type than the resource handle passed!");
			assert(handle.index < Size && "Resource array out of bounds!");
			assert(handle.version == versions[handle.index] && "Handle passed does not match internal handle!");
			return elements[handle.index];
		}

		template<typename U>
		void DestroyResource(QbVkResourceHandle<U> handle) {
			static_assert(eastl::is_same<T, U>::value, "Resource has a different type than the resource handle passed!");
			assert(handle.index < Size && "Resource array out of bounds!");
			assert(handle.version == versions[handle.index] && "Handle passed does not match internal handle!");
			versions[handle.index]++;
			freeList.push_back(handle.index);
		}

		QbVkResourceHandle<T> GetHandle(uint16_t index) {
			assert(index < (Size - 1) && "Resource array full, too many elements!");
			assert(index < 65536 && versions[index] < 65536 && "Resource array is out of handles to give out!");
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
			assert(next < 65536 && versions[next] < 65536 && "Resource array is out of handles to give out!");
			return QbVkResourceHandle<T> {next, versions[next]};
		}
	};

	struct QbVkVertex {
		glm::vec3 position{};
		glm::vec3 normal{};
		glm::vec2 uv0{};
		glm::vec2 uv1{};
		
		static VkVertexInputBindingDescription GetBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(QbVkVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}

		static eastl::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions() {
			eastl::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
			// Attribute description for POSITION
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(QbVkVertex, position);
			// Attribute description for NORMAL
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(QbVkVertex, normal);
			// Attribute description for TEXCOORD_0
			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(QbVkVertex, uv0);
			// Attribute description for TEXCOORD_1
			attributeDescriptions[3].binding = 0;
			attributeDescriptions[3].location = 3;	
			attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[3].offset = offsetof(QbVkVertex, uv1);
			return attributeDescriptions;
		}
	};

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
		VkDescriptorBufferInfo descriptor{};
	};

	struct QbVkImage {
		VkImage imgHandle = VK_NULL_HANDLE;
		QbVkAllocation alloc{};
	};

	struct QbVkTexture {
		QbVkImage image{};
		VkDescriptorImageInfo descriptor{};
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

		eastl::vector<VkSurfaceFormatKHR> surfaceFormats;
		eastl::vector<VkPresentModeKHR> presentModes;

		eastl::vector<VkQueueFamilyProperties> queueProps;
		eastl::vector<VkExtensionProperties> extensionProps;

		int graphicsFamilyIdx = -1;
		int presentFamilyIdx = -1;
		int computeFamilyIdx = -1;
	};

	struct Swapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkFormat imageFormat{};
		VkPresentModeKHR presentMode{};
		VkExtent2D extent{};
		eastl::vector<VkImage> images;
		eastl::vector<VkImageView> imageViews;
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

		eastl::unique_ptr<GPU> gpu;
		eastl::unique_ptr<QbVkAllocator> allocator;
		eastl::unique_ptr<QbVkResourceManager> resourceManager;
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

		eastl::array<RenderingResources, MAX_FRAMES_IN_FLIGHT> renderingResources;
	};

	struct QbVkRenderMeshInstance {
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;

		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		eastl::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};
	};

	using QbVkTextureHandle = QbVkResourceHandle<QbVkTexture>;
	using QbVkBufferHandle = QbVkResourceHandle<QbVkBuffer>;
	using QbVkDescriptorSetHandle = QbVkResourceHandle<VkDescriptorSet>;

	constexpr QbVkBufferHandle QBVK_BUFFER_NULL_HANDLE = { 65535, 65535 };
	constexpr QbVkTextureHandle QBVK_TEXTURE_NULL_HANDLE = { 65535, 65535 };
	constexpr QbVkDescriptorSetHandle QBVK_DESCRIPTORSET_NULL_HANDLE = { 65535, 65535 };

	struct QbVkTransfer {
		VkDeviceSize size = 0;
		QbVkBufferHandle destinationBuffer = QBVK_BUFFER_NULL_HANDLE;
	};

	template<typename T>
	struct QbVkMappedBuffer {
		QbVkBufferHandle handle = QBVK_BUFFER_NULL_HANDLE;
		T* data;
		T* operator->() {
			return data;
		}
	};
}