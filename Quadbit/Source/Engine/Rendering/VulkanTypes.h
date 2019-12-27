#pragma once

#include <cassert>

#include <EASTL/array.h>
#include <EASTL/deque.h>
#include <EASTL/hash_map.h>
#include <EASTL/string.h>
#include <EASTL/type_traits.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Engine/Core/Logging.h"
#include "Engine/Entities/EntityTypes.h"

#define VK_ERROR_STRING(x) case (int)x: return #x;

#define VK_CHECK(x) { \
VkResult ret = x; \
if(ret != VK_SUCCESS) QB_LOG_WARN("VkResult: %s is %s in %s at line %d\n", #x, VulkanErrorToString(x), __FILE__, __LINE__); \
}

#define VK_VALIDATE(x, msg) { \
if(!(x)) QB_LOG_WARN("VK: %s - %s\n", msg, #x); \
}

inline constexpr const char* VulkanErrorToString(VkResult vkResult) {
	switch (vkResult) {
		VK_ERROR_STRING(VK_SUCCESS);
		VK_ERROR_STRING(VK_NOT_READY);
		VK_ERROR_STRING(VK_TIMEOUT);
		VK_ERROR_STRING(VK_EVENT_SET);
		VK_ERROR_STRING(VK_EVENT_RESET);
		VK_ERROR_STRING(VK_INCOMPLETE);
		VK_ERROR_STRING(VK_ERROR_OUT_OF_HOST_MEMORY);
		VK_ERROR_STRING(VK_ERROR_OUT_OF_DEVICE_MEMORY);
		VK_ERROR_STRING(VK_ERROR_INITIALIZATION_FAILED);
		VK_ERROR_STRING(VK_ERROR_DEVICE_LOST);
		VK_ERROR_STRING(VK_ERROR_MEMORY_MAP_FAILED);
		VK_ERROR_STRING(VK_ERROR_LAYER_NOT_PRESENT);
		VK_ERROR_STRING(VK_ERROR_EXTENSION_NOT_PRESENT);
		VK_ERROR_STRING(VK_ERROR_FEATURE_NOT_PRESENT);
		VK_ERROR_STRING(VK_ERROR_INCOMPATIBLE_DRIVER);
		VK_ERROR_STRING(VK_ERROR_TOO_MANY_OBJECTS);
		VK_ERROR_STRING(VK_ERROR_FORMAT_NOT_SUPPORTED);
		VK_ERROR_STRING(VK_ERROR_SURFACE_LOST_KHR);
		VK_ERROR_STRING(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
		VK_ERROR_STRING(VK_SUBOPTIMAL_KHR);
		VK_ERROR_STRING(VK_ERROR_OUT_OF_DATE_KHR);
		VK_ERROR_STRING(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
		VK_ERROR_STRING(VK_ERROR_VALIDATION_FAILED_EXT);
		VK_ERROR_STRING(VK_ERROR_INVALID_SHADER_NV);
		VK_ERROR_STRING(VK_RESULT_BEGIN_RANGE);
		VK_ERROR_STRING(VK_RESULT_RANGE_SIZE);
	default: return "UNKNOWN";
	}
}

inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
inline constexpr size_t MAX_DESCRIPTORSETS_PER_INSTANCE = 4096;

namespace Quadbit {
	// At the moment we only support 65535 deletions of the same resource slot
	// And 65535 unique resources of the same type

	template<typename T>
	struct QbVkResourceHandle {
		uint32_t index : 16;
		uint32_t version : 16;

		bool operator==(const QbVkResourceHandle<T>& other) const {
			return index == other.index && version == other.version;
		}

		bool operator!=(const QbVkResourceHandle<T>& other) const {
			return index != other.index || version != other.version;
		}
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
			QB_ASSERT(handle.index < Size && "Resource array out of bounds!");
			QB_ASSERT(handle.version == versions[handle.index] && "Handle passed does not match internal handle!");
			return elements[handle.index];
		}

		template<typename U>
		void DestroyResource(QbVkResourceHandle<U> handle) {
			static_assert(eastl::is_same<T, U>::value, "Resource has a different type than the resource handle passed!");
			QB_ASSERT(handle.index < Size && "Resource array out of bounds!");
			QB_ASSERT(handle.version == versions[handle.index] && "Handle passed does not match internal handle!");
			versions[handle.index]++;
			freeList.push_back(handle.index);
		}

		QbVkResourceHandle<T> GetHandle(uint16_t index) {
			QB_ASSERT(index < (Size - 1) && "Resource array full, too many elements!");
			QB_ASSERT(index < 65536 && versions[index] < 65536 && "Resource array is out of handles to give out!");
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
			QB_ASSERT(next < (Size - 1) && "Resource array full, too many elements!");
			QB_ASSERT(next < 65536 && versions[next] < 65536 && "Resource array is out of handles to give out!");
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

	struct QbVkDescriptorAllocator {
		size_t descriptorIndex = 0;
		VkDescriptorPool pool;
		QbVkResource<eastl::vector<VkDescriptorSet>, MAX_DESCRIPTORSETS_PER_INSTANCE> setInstances;
	};

	class QbVkPipeline;

	using QbVkBufferHandle = QbVkResourceHandle<QbVkBuffer>;
	using QbVkTextureHandle = QbVkResourceHandle<QbVkTexture>;
	using QbVkDescriptorAllocatorHandle = QbVkResourceHandle<QbVkDescriptorAllocator>;
	using QbVkDescriptorSetsHandle = QbVkResourceHandle<eastl::vector<VkDescriptorSet>>;
	using QbVkPipelineHandle = QbVkResourceHandle<eastl::unique_ptr<QbVkPipeline>>;

	constexpr QbVkBufferHandle QBVK_BUFFER_NULL_HANDLE = { 65535, 65535 };
	constexpr QbVkTextureHandle QBVK_TEXTURE_NULL_HANDLE = { 65535, 65535 };
	constexpr QbVkDescriptorAllocatorHandle QBVK_DESCRIPTOR_ALLOCATOR_NULL_HANDLE = { 65535, 65535 };
	constexpr QbVkDescriptorSetsHandle QBVK_DESCRIPTOR_SETS_NULL_HANDLE = { 65535, 65535 };
	constexpr QbVkPipelineHandle QBVK_PIPELINE_NULL_HANDLE = { 65535, 65535 };

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

	struct ShadowmapResources {
		QbVkTextureHandle texture = QBVK_TEXTURE_NULL_HANDLE;
		QbVkPipelineHandle pipeline = QBVK_PIPELINE_NULL_HANDLE;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		eastl::array<VkFramebuffer, 2> framebuffers;
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
	class QbVkShaderCompiler;
	class QbVkResourceManager;
	class EntityManager;
	class InputHandler;
	struct QbVkContext {
		InputHandler* inputHandler;
		EntityManager* entityManager;

		eastl::unique_ptr<GPU> gpu;
		eastl::unique_ptr<QbVkAllocator> allocator;
		eastl::unique_ptr<QbVkShaderCompiler> shaderCompiler;
		eastl::unique_ptr<QbVkResourceManager> resourceManager;
		VkDevice device = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		VkQueue presentQueue = VK_NULL_HANDLE;
		VkQueue computeQueue = VK_NULL_HANDLE;

		uint32_t resourceIndex = 0;
		eastl::array<RenderingResources, MAX_FRAMES_IN_FLIGHT> renderingResources;

		MSAAResources multisamplingResources;
		DepthResources depthResources{};
		ShadowmapResources shadowmapResources{};
		Swapchain swapchain{};

		VkCommandPool commandPool = VK_NULL_HANDLE;

		VkRenderPass mainRenderPass = VK_NULL_HANDLE;

		eastl::hash_map<eastl::string, double> computeAvgTimes;

		Entity fallbackCamera = NULL_ENTITY;
		Entity userCamera = NULL_ENTITY;

		float sunAzimuth;
		float sunAltitude;

		Entity GetActiveCamera() {
			return (userCamera == NULL_ENTITY) ? fallbackCamera : userCamera;
		}

		void SetCamera(Entity entity) {
			QB_ASSERT(entity != NULL_ENTITY && "Invalid camera entity!");
			userCamera = entity;
		}
	};

	struct QbVkTransfer {
		QbVkBufferHandle destinationBuffer = QBVK_BUFFER_NULL_HANDLE;
		VkDeviceSize size = 0;
	};

	template<typename T>
	struct QbVkUniformBuffer {
		QbVkBufferHandle handle = QBVK_BUFFER_NULL_HANDLE;
		size_t alignedSize = 0;
	};

	enum class QbVkShaderType {
		QBVK_SHADER_TYPE_FRAGMENT,
		QBVK_SHADER_TYPE_VERTEX,
		QBVK_SHADER_TYPE_COMPUTE
	};

	constexpr VkSamplerCreateInfo DEFAULT_TEXTURE_SAMPLER {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 16.0f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE
	};
}